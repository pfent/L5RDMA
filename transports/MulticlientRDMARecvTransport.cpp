#include "include/MulticlientRDMARecvTransport.h"
#include "util/socket/tcp.h"
#include <cassert>

namespace l5::transport {
using namespace util;

MulticlientRDMARecvTransportServer::MulticlientRDMARecvTransportServer(const std::string& port, size_t maxClients)
   : MAX_CLIENTS(maxClients),
     listenSock(Socket::create()),
     net(),
     sharedCq(&net.getSharedCompletionQueue()),
     receives(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
     sendBuffer(MAX_MESSAGESIZE, net, {}) {
   listen(std::stoi(port));
}

void MulticlientRDMARecvTransportServer::listen(uint16_t port) {
   tcp::bind(listenSock, port);
   tcp::listen(listenSock);
}

void MulticlientRDMARecvTransportServer::accept() {
   const auto clientId = connections.size();

   auto acced = tcp::accept(listenSock);

   auto qp = rdma::RcQueuePair(net);

   auto address = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
   tcp::write(acced, address);
   tcp::read(acced, address);

   auto receiveAddr = receives.getAddr().offset(sizeof(uint8_t[MAX_MESSAGESIZE]) * clientId);
   tcp::write(acced, receiveAddr);
   tcp::read(acced, receiveAddr);

   qp.connect(address);

   auto recv = ibv::workrequest::Recv{};
   recv.setId(connections.size()); // TODO: could we use the ID to identify the client here?

   auto answer = ibv::workrequest::Simple<ibv::workrequest::Write>();
   answer.setLocalAddress(sendBuffer.getSlice());
   answer.setRemoteAddress(receiveAddr);
   answer.setInline();
   answer.setSignaled();

   // map QueuePairNumber to client id
   qpnToConnection[qp.getQPN()] = connections.size();
   auto& connection = connections.emplace_back(std::move(acced), std::move(qp), answer, recv);
   // immediately post a recv to ensure that we always have a receive request ready for each accepted connection
   connection.qp.postRecvRequest(connection.recv);
}

size_t MulticlientRDMARecvTransportServer::receive(void* whereTo, size_t maxSize) {
   auto wc = net.getSharedCompletionQueue().pollRecvWorkCompletionBlocking();
   // find out, which client this message came from
   auto client = qpnToConnection.at(wc.getQueuePairNumber());
   auto& connection = connections[client];
   // immediately replace the consumed receive request
   connection.qp.postRecvRequest(connection.recv);

   // copy message + size
   const auto sizePtr = reinterpret_cast<uint8_t*>(receives.data()[client]);
   const auto size = *reinterpret_cast<size_t*>(sizePtr);
   if (maxSize < size) {
      throw std::runtime_error("received message > maxSize");
   }
   const auto begin = sizePtr + sizeof(size_t);
   const auto end = begin + size;

   std::copy(begin, end, reinterpret_cast<uint8_t*>(whereTo));
   return client;
}

void MulticlientRDMARecvTransportServer::send(size_t receiverId, const uint8_t* data, size_t size) {
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
   // TODO: selective signaling needs to happen per queue pair / connection
   sendCounter++;
   if (sendCounter % 1024 == 0) { // selective signaling
      con.answerWr.setFlags(getWrFlags(true, totalLength < 512));
      con.qp.postWorkRequest(con.answerWr);
      sharedCq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
   } else {
      con.answerWr.setFlags(getWrFlags(false, totalLength < 512));
      con.qp.postWorkRequest(con.answerWr);
   }
}

void MulticlientRDMARecvTransportServer::finishListen() {
   listenSock.close();
}

MulticlientRDMARecvTransportClient::MulticlientRDMARecvTransportClient()
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

void MulticlientRDMARecvTransportClient::rdmaConnect() {
   auto address = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
   tcp::write(sock, address);
   tcp::read(sock, address);

   auto receiveAddr = receiveBuffer.getAddr();
   tcp::write(sock, receiveAddr);
   tcp::read(sock, receiveAddr);

   qp.connect(address);

   dataWr.setRemoteAddress(receiveAddr);
}

void MulticlientRDMARecvTransportClient::connect(std::string_view whereTo) {
   const auto pos = whereTo.find(':');
   if (pos == std::string::npos) {
      throw std::runtime_error("usage: <0.0.0.0:port>");
   }
   const auto ip = std::string(whereTo.data(), pos);
   const auto port = std::stoi(std::string(whereTo.begin() + pos + 1, whereTo.end()));
   return connect(ip, port);
}

void MulticlientRDMARecvTransportClient::connect(const std::string& ip, uint16_t port) {
   tcp::connect(sock, ip, port);

   rdmaConnect();
}

void MulticlientRDMARecvTransportClient::send(const uint8_t* data, size_t size) {
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

size_t MulticlientRDMARecvTransportClient::receive(void* whereTo, size_t maxSize) {
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
