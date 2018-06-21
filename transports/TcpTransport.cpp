#include <string>
#include <arpa/inet.h>
#include "include/TcpTransport.h"
#include "util/socket/tcp.h"

namespace l5 {
namespace transport {
using namespace util;

TcpTransportServer::TcpTransportServer(const std::string &port) :
        initialSocket(Socket::create()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

TcpTransportServer::~TcpTransportServer() = default;

void TcpTransportServer::listen(uint16_t port) {
    tcp::bind(initialSocket, port);
    tcp::listen(initialSocket);
}

void TcpTransportServer::write_impl(const uint8_t *data, size_t size) {
    tcp::write(communicationSocket, data, size);
}

void TcpTransportServer::read_impl(uint8_t *buffer, size_t size) {
    tcp::read(communicationSocket, buffer, size);
}

void TcpTransportServer::accept_impl() {
    communicationSocket = tcp::accept(initialSocket);
}

TcpTransportClient::TcpTransportClient() : socket(Socket::create()) {}

TcpTransportClient::~TcpTransportClient() = default;

void TcpTransportClient::connect_impl(const std::string &connection) {
    const auto pos = connection.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(connection.data(), pos);
    const auto port = std::stoi(std::string(connection.begin() + pos + 1, connection.end()));

    tcp::connect(socket, ip, port);
}

void TcpTransportClient::write_impl(const uint8_t *data, size_t size) {
    tcp::write(socket, data, size);
}

void TcpTransportClient::read_impl(uint8_t *buffer, size_t size) {
    tcp::read(socket, buffer, size);
}
} // namespace transport
} // namespace l5
