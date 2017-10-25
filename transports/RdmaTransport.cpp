#include <rdma_tests/util/tcpWrapper.h>
#include <arpa/inet.h>
#include "RdmaTransport.h"

RdmaTransport::RdmaTransport(std::string_view port) :
        sock(tcp_socket()),
        port(std::stoi(std::string(port.data(), port.size()))) {}

RdmaTransport::~RdmaTransport() {
    tcp_close(sock);
}

void RdmaTransport::connect(std::string_view ip) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, std::string(ip.data(), ip.size()).data(), &addr.sin_addr);

    tcp_connect(sock, addr);
    rdma = std::make_unique<RDMAMessageBuffer>(1024 * 1024 * 10, sock); // 10MB
}

void RdmaTransport::accept() {
    sockaddr_in ignored{};
    auto acced = tcp_accept(sock, ignored);
    rdma = std::make_unique<RDMAMessageBuffer>(1024 * 1024 * 10, acced); // same as in connect
}

void RdmaTransport::listen() {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(sock, addr);
    tcp_listen(sock);
}

void RdmaTransport::write(const uint8_t *data, size_t size) {
    rdma->send(data, size);
}

void RdmaTransport::read(uint8_t *buffer, size_t size) {
    rdma->receive(buffer, size);
}


