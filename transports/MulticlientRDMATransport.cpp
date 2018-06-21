#include <netinet/in.h>
#include <boost/assert.hpp>
#include "include/MulticlientRDMATransport.h"
#include "util/socket/tcp.h"

namespace l5 {
namespace transport {
using namespace util;

MulticlientRDMATransportServer::MulticlientRDMATransportServer(const std::string &port, size_t maxClients)
        : MAX_CLIENTS(maxClients),
          listenSock(Socket::create()),
          net(),
          sharedCq(&net.getSharedCompletionQueue()),
          receives(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          doorBells(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          sendBuffer(MAX_MESSAGESIZE, net, {}) {
    assert(maxClients % 16 == 0);
    std::fill(doorBells.begin(), doorBells.end(), '\0');
    listen(std::stoi(port));
}

void MulticlientRDMATransportServer::listen(uint16_t port) {
    tcp::bind(listenSock, port);
    tcp::listen(listenSock);
}

void MulticlientRDMATransportServer::accept() {
    const auto clientId = connections.size();

    auto acced = tcp::accept(listenSock);

    auto qp = rdma::RcQueuePair(net);

    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp::write(acced, address);
    tcp::read(acced, address);

    auto receiveAddr = receives.getAddr().offset(sizeof(uint8_t[MAX_MESSAGESIZE]) * clientId);
    tcp::write(acced, receiveAddr);
    tcp::read(acced, receiveAddr);

    auto doorBellAddr = doorBells.getAddr().offset(sizeof(char) * clientId);
    tcp::write(acced, doorBellAddr);

    qp.connect(address);

    auto answer = ibv::workrequest::Simple<ibv::workrequest::Write>();
    answer.setLocalAddress(sendBuffer.getSlice());
    answer.setRemoteAddress(receiveAddr);
    answer.setInline();
    answer.setSignaled();

    connections.push_back(Connection{std::move(acced), std::move(qp), answer});
}

MulticlientRDMATransportServer::~MulticlientRDMATransportServer() = default;

size_t MulticlientRDMATransportServer::receive(void *whereTo, size_t maxSize) {
    size_t res;
    receive([&](auto sender, auto begin, auto end) {
        res = sender;
        const auto size = static_cast<size_t>(std::distance(begin, end));
        if (maxSize < size) {
            throw std::runtime_error("received message > maxSize");
        }
        std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));
    });
    return res;
}

void MulticlientRDMATransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    const auto totalLength = size + sizeof(size_t) + sizeof(validity);
    if (totalLength > MAX_MESSAGESIZE) {
        throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
    }
    if (receiverId > connections.size()) {
        throw std::runtime_error("no such connection");
    }

    send(receiverId, [&](auto begin) {
        std::copy(data, data + size, begin);
        return size;
    });
}

void MulticlientRDMATransportServer::finishListen() {
    listenSock.close();
}

MultiClientRDMATransportClient::MultiClientRDMATransportClient()
        : sock(Socket::create()),
          net(),
          cq(net.getSharedCompletionQueue()),
          qp(rdma::RcQueuePair(net)),
          sendBuffer(MAX_MESSAGESIZE, net, {}),
          doorBell(1, net, {}),
          receiveBuffer(MAX_MESSAGESIZE, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          dataWr(),
          doorBellWr() {
    dataWr.setLocalAddress(sendBuffer.getSlice());
    dataWr.setSignaled();
    dataWr.setInline();

    doorBellWr.setLocalAddress(doorBell.getSlice());
    doorBellWr.setSignaled();
    doorBellWr.setInline();
}

void MultiClientRDMATransportClient::rdmaConnect() {
    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp::write(sock, address);
    tcp::read(sock, address);

    auto receiveAddr = receiveBuffer.getAddr();
    tcp::write(sock, receiveAddr);
    tcp::read(sock, receiveAddr);

    auto doorBellAddr = ibv::memoryregion::RemoteAddress();
    tcp::read(sock, doorBellAddr);

    qp.connect(address);

    dataWr.setRemoteAddress(receiveAddr);
    doorBellWr.setRemoteAddress(doorBellAddr);
}

void MultiClientRDMATransportClient::connect(const std::string &ip, uint16_t port) {
    tcp::connect(sock, ip, port);

    rdmaConnect();
}

void MultiClientRDMATransportClient::send(const uint8_t *data, size_t size) {
    const auto dataWrSize = size + sizeof(size_t);
    if (dataWrSize > MAX_MESSAGESIZE) {
        throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
    }

    send([&](auto begin) {
        std::copy(data, data + size, begin);
        return size;
    });
}

size_t MultiClientRDMATransportClient::receive(void *whereTo, size_t maxSize) {
    size_t size;
    receive([&](auto begin, auto end) {
        size = static_cast<size_t>(std::distance(begin, end));
        if (size > maxSize) {
            throw std::runtime_error("received message > maxSize");
        }
        std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));
    });
    return size;
}
} // namespace transport
} // namespace l5
