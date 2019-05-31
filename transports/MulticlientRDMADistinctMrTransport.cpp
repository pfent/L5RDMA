#include "include/MulticlientRDMADistinctMrTransport.h"
#include "util/socket/tcp.h"
#include <cassert>

namespace l5::transport {
using namespace util;

MulticlientRDMADistinctMrTransportServer::MulticlientRDMADistinctMrTransportServer(const std::string& port, size_t maxClients)
   : MAX_CLIENTS(maxClients),
     listenSock(Socket::create()),
     net(),
     sharedCq(&net.getSharedCompletionQueue()),
     receives(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
     sendBuffer(MAX_MESSAGESIZE, net, {}) {
   listen(std::stoi(port));
}

void MulticlientRDMADistinctMrTransportServer::listen(uint16_t port) {
   tcp::bind(listenSock, port);
   tcp::listen(listenSock);
}

void MulticlientRDMADistinctMrTransportServer::accept() {
   const auto clientId = connections.size();
   auto acced = tcp::accept(listenSock);
   auto qp = rdma::RcQueuePair(net);

   auto answer = ibv::workrequest::Simple<ibv::workrequest::Write>();
   auto& connection = connections.emplace_back(std::move(acced), std::move(qp), answer);

   auto address = rdma::Address{net.getGID(), connection.qp.getQPN(), net.getLID()};
   tcp::write(connection.socket, address);
   tcp::read(connection.socket, address);

   auto receiveAddr = receives.getAddr().offset(sizeof(uint8_t[MAX_MESSAGESIZE]) * clientId);
   tcp::write(connection.socket, receiveAddr);
   tcp::read(connection.socket, receiveAddr);

   connection.answerWr.setLocalAddress(sendBuffer.getSlice());
   connection.answerWr.setRemoteAddress(receiveAddr);
   connection.answerWr.setInline();
   connection.answerWr.setSignaled();

   connection.qp.connect(address);
}

size_t MulticlientRDMADistinctMrTransportServer::receive(void* whereTo, size_t maxSize) {
   // poll all receive regions
   size_t client = [&] {
      for (;;) {
         for (size_t i = 0; i < connections.size(); ++i) {
            if (*reinterpret_cast<volatile size_t*>(&receives.data()[i]) != '\0') {
               return i;
            }
         }
      }
   }();

   // copy message + size
   const auto sizePtr = reinterpret_cast<uint8_t*>(receives.data()[client]);
   const auto size = *reinterpret_cast<size_t*>(sizePtr);
   if (maxSize < size) {
      throw std::runtime_error("received message > maxSize");
   }
   const auto begin = sizePtr + sizeof(size_t);
   const auto end = begin + size;

   std::copy(begin, end, reinterpret_cast<uint8_t*>(whereTo));

   // reset the size to allow the next write
   *sizePtr = 0;
   return client;
}

void MulticlientRDMADistinctMrTransportServer::send(size_t receiverId, const uint8_t* data, size_t size) {
   const auto totalLength = size + sizeof(size_t) + sizeof(validity);
   if (totalLength > MAX_MESSAGESIZE) {
      throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
   }
   if (receiverId > connections.size()) {
      throw std::runtime_error("no such connection");
   }

   auto& con = connections[receiverId];

   auto sizePtr = reinterpret_cast<size_t*>(sendBuffer.data());
   auto begin = sendBuffer.data() + sizeof(size_t);

   std::copy(data, data + size, begin);

   auto validityPtr = sendBuffer.data() + sizeof(size_t) + size;

   *sizePtr = size;
   *validityPtr = validity;

   con.answerWr.setLocalAddress(sendBuffer.getSlice(0, totalLength));
   // selective signaling needs to happen per queuepair / connection
   ++con.sendCounter;
   if (con.sendCounter % 1024 == 0) {
      con.answerWr.setFlags(getWrFlags(true, totalLength < 512));
      con.qp.postWorkRequest(con.answerWr);
      sharedCq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
   } else {
      con.answerWr.setFlags(getWrFlags(false, totalLength < 512));
      con.qp.postWorkRequest(con.answerWr);
   }
}

void MulticlientRDMADistinctMrTransportServer::finishListen() {
   listenSock.close();
}

MulticlientRDMADistinctMrTransportClient::MulticlientRDMADistinctMrTransportClient()
   : sock(Socket::create()),
     net(),
     cq(net.getSharedCompletionQueue()),
     qp(rdma::RcQueuePair(net)),
     sendBuffer(MAX_MESSAGESIZE, net, {}),
     receiveBuffer(MAX_MESSAGESIZE, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
     dataWr() {
   dataWr.setSignaled();
   dataWr.setInline();
}

void MulticlientRDMADistinctMrTransportClient::rdmaConnect() {
   auto address = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
   tcp::write(sock, address);
   tcp::read(sock, address);

   auto receiveAddr = receiveBuffer.getAddr();
   tcp::write(sock, receiveAddr);
   tcp::read(sock, receiveAddr);

   qp.connect(address);

   dataWr.setRemoteAddress(receiveAddr);
}

void MulticlientRDMADistinctMrTransportClient::connect(std::string_view whereTo) {
   const auto pos = whereTo.find(':');
   if (pos == std::string::npos) {
      throw std::runtime_error("usage: <0.0.0.0:port>");
   }
   const auto ip = std::string(whereTo.data(), pos);
   const auto port = std::stoi(std::string(whereTo.begin() + pos + 1, whereTo.end()));
   return connect(ip, port);
}

void MulticlientRDMADistinctMrTransportClient::connect(const std::string& ip, uint16_t port) {
   tcp::connect(sock, ip, port);

   rdmaConnect();
}

void MulticlientRDMADistinctMrTransportClient::send(const uint8_t* data, size_t size) {
   const auto dataWrSize = size + sizeof(size_t);
   if (dataWrSize > MAX_MESSAGESIZE) {
      throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
   }

   auto sizePtr = reinterpret_cast<size_t*>(sendBuffer.data());
   *sizePtr = size;
   auto payloadBegin = sendBuffer.data() + sizeof(size_t);

   std::copy(data, data + size, payloadBegin);
   dataWr.setLocalAddress(sendBuffer.getSlice(0, dataWrSize));
   qp.postWorkRequest(dataWr);
   cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

size_t MulticlientRDMADistinctMrTransportClient::receive(void* whereTo, size_t maxSize) {
   size_t size;
   do {
      size = *reinterpret_cast<volatile size_t*>(receiveBuffer.data());
   } while (size == 0 || *reinterpret_cast<volatile char*>(receiveBuffer.data() + sizeof(size_t) + size) != validity);
   if (size > maxSize) {
      throw std::runtime_error("received message > maxSize");
   }
   const auto begin = receiveBuffer.data() + sizeof(size_t);
   const auto end = begin + size;

   std::copy(begin, end, reinterpret_cast<uint8_t*>(whereTo));
   *reinterpret_cast<volatile size_t*>(receiveBuffer.data()) = 0;
   return size;
}
} // namespace l5
