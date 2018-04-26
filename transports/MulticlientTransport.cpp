#include <util/tcpWrapper.h>
#include <netinet/in.h>
#include <boost/assert.hpp>
#include "MulticlientTransport.h"

MulticlientTransportServer::MulticlientTransportServer(std::string_view port, size_t maxClients)
        : MAX_CLIENTS(maxClients),
          listenSock(tcp_socket()),
          net(rdma::Network()),
          sharedCq(net.getSharedCompletionQueue()),
          receives(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          doorBells(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          sendBuffer(MAX_MESSAGESIZE, net, {}) {
    assert(maxClients % 16 == 0);
    std::fill(doorBells.begin(), doorBells.end(), '\0');
    listen(std::stoi(std::string(port.data(), port.size())));
}

void MulticlientTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(listenSock, addr);
    tcp_listen(listenSock);
}

void MulticlientTransportServer::accept() {
    const auto clientId = connections.size();

    auto ignored = sockaddr_in{};
    auto acced = tcp_accept(listenSock, ignored);

    auto qp = rdma::RcQueuePair(net);

    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(acced, address);
    tcp_read(acced, address);

    auto receiveAddr = receives.getAddr().offset(sizeof(uint8_t[MAX_MESSAGESIZE]) * clientId);
    tcp_write(acced, receiveAddr);
    tcp_read(acced, receiveAddr);

    auto doorBellAddr = doorBells.getAddr().offset(sizeof(char) * clientId);
    tcp_write(acced, doorBellAddr);

    qp.connect(address);

    auto answer = ibv::workrequest::Simple<ibv::workrequest::Write>();
    answer.setLocalAddress(sendBuffer.getSlice());
    answer.setRemoteAddress(receiveAddr);
    answer.setInline();
    answer.setSignaled();

    connections.push_back(Connection{acced, std::move(qp), answer});
}

MulticlientTransportServer::~MulticlientTransportServer() {
    for (const auto &conn : connections) {
        if (conn.socket != -1) {
            tcp_close(conn.socket);
        }
    }
    tcp_close(listenSock);
}

size_t MulticlientTransportServer::receive(void *whereTo, size_t maxSize) {
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

void MulticlientTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
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

MultiClientTransportClient::MultiClientTransportClient()
        : sock(tcp_socket()),
          net(rdma::Network()),
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

void MultiClientTransportClient::rdmaConnect() {
    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(sock, address);
    tcp_read(sock, address);

    auto receiveAddr = receiveBuffer.getAddr();
    tcp_write(sock, receiveAddr);
    tcp_read(sock, receiveAddr);

    auto doorBellAddr = ibv::memoryregion::RemoteAddress();
    tcp_read(sock, doorBellAddr);

    qp.connect(address);

    dataWr.setRemoteAddress(receiveAddr);
    doorBellWr.setRemoteAddress(doorBellAddr);
}

void MultiClientTransportClient::connect(std::string_view whereTo) {
    tcp_connect(sock, whereTo);

    rdmaConnect();
}

void MultiClientTransportClient::connect(std::string_view ip, uint16_t port) {
    tcp_connect(sock, std::string(ip), port);

    rdmaConnect();
}

void MultiClientTransportClient::send(const uint8_t *data, size_t size) {
    const auto dataWrSize = size + sizeof(size_t);
    if (dataWrSize > MAX_MESSAGESIZE) {
        throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
    }

    send([&](auto begin) {
        std::copy(data, data + size, begin);
        return size;
    });
}

size_t MultiClientTransportClient::receive(void *whereTo, size_t maxSize) {
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
