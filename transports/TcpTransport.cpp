#include <string>
#include <arpa/inet.h>
#include "rdma_tests/util/tcpWrapper.h"
#include "TcpTransport.h"

TcpTransport::TcpTransport(std::string_view port) :
        initialSocket(tcp_socket()),
        port(std::stoi(std::string(port.data(), port.size()))) {}

TcpTransport::~TcpTransport() {
    tcp_close(initialSocket);

    if (initialSocket != communicationSocket && communicationSocket != -1) {
        tcp_close(communicationSocket);
    }
}

void TcpTransport::connect(std::string_view ip) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, std::string(ip.data(), ip.size()).data(), &addr.sin_addr);

    tcp_connect(initialSocket, addr);
    communicationSocket = initialSocket;
}

void TcpTransport::listen() {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(initialSocket, addr);
    tcp_listen(initialSocket);
}

void TcpTransport::write(const uint8_t *data, size_t size) {
    tcp_write(communicationSocket, data, size);
}

void TcpTransport::read(uint8_t *buffer, size_t size) {
    tcp_read(communicationSocket, buffer, size);
}

void TcpTransport::accept() {
    sockaddr_in ignored{};
    communicationSocket = tcp_accept(initialSocket, ignored);
}
