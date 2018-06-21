#include <arpa/inet.h>
#include "util/socket/tcp.h"
#include "include/RdmaTransport.h"

namespace l5 {
namespace transport {
using namespace util;

RdmaTransportServer::RdmaTransportServer(const std::string &port) :
        sock(Socket::create()) {
    auto p = std::stoi(port);
    listen(p);
}

RdmaTransportServer::~RdmaTransportServer() = default;

void RdmaTransportServer::accept_impl() {
    auto acced = tcp::accept(sock);
    rdma = std::make_unique<datastructure::VirtualRDMARingBuffer>(BUFFER_SIZE, acced);
}

void RdmaTransportServer::listen(uint16_t port) {
    tcp::bind(sock, port);
    tcp::listen(sock);
}

void RdmaTransportServer::write_impl(const uint8_t *data, size_t size) {
    rdma->send(data, size);
}

void RdmaTransportServer::read_impl(uint8_t *buffer, size_t size) {
    rdma->receive(buffer, size);
}

void RdmaTransportClient::connect_impl(const std::string &connection) {
    const auto pos = connection.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(connection.data(), pos);
    const auto port = std::stoi(std::string(connection.begin() + pos + 1, connection.end()));

    tcp::connect(sock, ip, port);
    rdma = std::make_unique<datastructure::VirtualRDMARingBuffer>(BUFFER_SIZE, sock);
}

RdmaTransportClient::RdmaTransportClient() : sock(Socket::create()) {}

RdmaTransportClient::~RdmaTransportClient() = default;

void RdmaTransportClient::write_impl(const uint8_t *data, size_t size) {
    rdma->send(data, size);
}

void RdmaTransportClient::read_impl(uint8_t *buffer, size_t size) {
    rdma->receive(buffer, size);
}
} // namespace transport
} // namespace l5
