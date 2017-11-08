#include <string>
#include <arpa/inet.h>
#include "exchangeableTransports/util/tcpWrapper.h"
#include "TcpTransport.h"

TcpTransportServer::TcpTransportServer(std::string_view port) :
        initialSocket(tcp_socket()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

TcpTransportServer::~TcpTransportServer() {
    tcp_close(initialSocket);

    if (communicationSocket != -1) {
        tcp_close(communicationSocket);
    }
}

void TcpTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(initialSocket, addr);
    tcp_listen(initialSocket);
}

void TcpTransportServer::write_impl(const uint8_t *data, size_t size) {
    tcp_write(communicationSocket, data, size);
}

void TcpTransportServer::read_impl(uint8_t *buffer, size_t size) {
    tcp_read(communicationSocket, buffer, size);
}

void TcpTransportServer::accept_impl() {
    sockaddr_in ignored{};
    communicationSocket = tcp_accept(initialSocket, ignored);
}

TcpTransportClient::TcpTransportClient() : socket(tcp_socket()) {}

TcpTransportClient::~TcpTransportClient() {
    tcp_close(socket);
}

void TcpTransportClient::connect_impl(std::string_view connection) {
    const auto pos = connection.find(':');
    const auto ip = std::string(connection.data(), pos);
    const auto port = std::stoi(std::string(connection.begin() + pos, connection.end()));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);

    tcp_connect(socket, addr);
}

void TcpTransportClient::write_impl(const uint8_t *data, size_t size) {
    tcp_write(socket, data, size);
}

void TcpTransportClient::read_impl(uint8_t *buffer, size_t size) {
    tcp_read(socket, buffer, size);
}
