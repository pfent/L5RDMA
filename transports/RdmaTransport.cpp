#include <exchangeableTransports/util/tcpWrapper.h>
#include <arpa/inet.h>
#include "RdmaTransport.h"

RdmaTransportServer::RdmaTransportServer(std::string_view port) :
        sock(tcp_socket()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

RdmaTransportServer::~RdmaTransportServer() {
    tcp_close(sock);
}

void RdmaTransportServer::accept_impl() {
    sockaddr_in ignored{};
    auto acced = tcp_accept(sock, ignored);
    rdma = std::make_unique<RDMAMessageBuffer>(1024 * 1024 * 10, acced); // same as in connect
}

void RdmaTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(sock, addr);
    tcp_listen(sock);
}

void RdmaTransportServer::write_impl(const uint8_t *data, size_t size) {
    rdma->send(data, size);
}

void RdmaTransportServer::read_impl(uint8_t *buffer, size_t size) {
    rdma->receive(buffer, size);
}

void RdmaTransportClient::connect_impl(std::string_view ip) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, std::string(ip.data(), ip.size()).data(), &addr.sin_addr);

    tcp_connect(sock, addr);
    rdma = std::make_unique<RDMAMessageBuffer>(1024 * 1024 * 10, sock); // 10MB
}


