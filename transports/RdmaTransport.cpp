#include <arpa/inet.h>
#include "util/tcpWrapper.h"
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
    rdma = std::make_unique<RdmaMemoryDatastructure>(BUFFER_SIZE, acced);
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

Buffer RdmaTransportServer::getBuffer_impl(size_t) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void RdmaTransportServer::write_impl(Buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

Buffer RdmaTransportServer::read_impl(size_t) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void RdmaTransportServer::markAsRead_impl(Buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void RdmaTransportClient::connect_impl(std::string_view connection) {
    const auto pos = connection.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(connection.data(), pos);
    const auto port = std::stoi(std::string(connection.begin() + pos + 1, connection.end()));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    tcp_connect(sock, addr);
    rdma = std::make_unique<RdmaMemoryDatastructure>(BUFFER_SIZE, sock);
}

RdmaTransportClient::RdmaTransportClient() : sock(tcp_socket()) {}

RdmaTransportClient::~RdmaTransportClient() {
    tcp_close(sock);
}

void RdmaTransportClient::write_impl(const uint8_t *data, size_t size) {
    rdma->send(data, size);
}

void RdmaTransportClient::read_impl(uint8_t *buffer, size_t size) {
    rdma->receive(buffer, size);
}

Buffer RdmaTransportClient::getBuffer_impl(size_t) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void RdmaTransportClient::write_impl(Buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

Buffer RdmaTransportClient::read_impl(size_t) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void RdmaTransportClient::markAsRead_impl(Buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}


