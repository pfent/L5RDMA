#include <string>
#include <util/tcpWrapper.h>
#include <arpa/inet.h>
#include <cassert>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include "MulticlientTCPTransport.h"

using namespace std::string_literals;

MulticlientTCPTransportServer::MulticlientTCPTransportServer(std::string_view port) :
        serverSocket(tcp_socket()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

void MulticlientTCPTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(serverSocket, addr);
    tcp_listen(serverSocket);
}

MulticlientTCPTransportServer::~MulticlientTCPTransportServer() {
    tcp_close(serverSocket);
    for (auto connection : connections) {
        tcp_close(connection);
    }
}

void MulticlientTCPTransportServer::accept() {
    sockaddr_in ignored{};
    connections.push_back(tcp_accept(serverSocket, ignored));
    pollfd p{};
    p.fd = connections.back();
    p.events = POLLIN;
    pollFds.push_back(p);
}

void MulticlientTCPTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    assert(receiverId < connections.size());
    tcp_write(connections[receiverId], data, size);
}

size_t MulticlientTCPTransportServer::receive(void *whereTo, size_t maxSize) {
    const auto ret = ::poll(pollFds.data(), pollFds.size(), 5 * 1000); // 5 seconds timeout
    if (ret < 0) {
        throw std::runtime_error("Could not poll sockets: "s + ::strerror(errno));
    }
    const auto &readable = std::find_if(pollFds.begin(), pollFds.end(), [](const pollfd &pollFd) {
        return (pollFd.revents & POLLIN) != 0;
    });

    tcp_read(readable->fd, whereTo, maxSize);

    return static_cast<size_t>(std::distance(pollFds.begin(), readable));
}

MulticlientTCPTransportClient::MulticlientTCPTransportClient() : socket(tcp_socket()) {

}

MulticlientTCPTransportClient::~MulticlientTCPTransportClient() {
    tcp_close(socket);
}

void MulticlientTCPTransportClient::connect(const std::string &ip, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    tcp_connect(socket, addr);
}

void MulticlientTCPTransportClient::connect(std::string_view whereTo) {
    const auto pos = whereTo.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(whereTo.data(), pos);
    const auto port = std::stoi(std::string(whereTo.begin() + pos + 1, whereTo.end()));
    return connect(ip, port);
}

void MulticlientTCPTransportClient::send(const uint8_t *data, size_t size) {
    tcp_write(socket, data, size);
}

void MulticlientTCPTransportClient::receive(void *whereTo, size_t maxSize) {
    return tcp_read(socket, whereTo, maxSize);
}
