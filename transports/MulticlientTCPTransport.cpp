#include <string>
#include <arpa/inet.h>
#include <cassert>
#include "util/socket/tcp.h"
#include "include/MulticlientTCPTransport.h"

namespace l5 {
namespace transport {
using namespace std::string_literals;
using namespace util;

MulticlientTCPTransportServer::MulticlientTCPTransportServer(std::string_view port) :
        serverSocket(Socket::create()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

void MulticlientTCPTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp::bind(serverSocket, addr);
    tcp::listen(serverSocket);
}

MulticlientTCPTransportServer::~MulticlientTCPTransportServer() = default;

void MulticlientTCPTransportServer::accept() {
    sockaddr_in ignored{};
    connections.push_back(tcp::accept(serverSocket, ignored));
    pollfd p{};
    p.fd = connections.back().get();
    p.events = POLLIN;
    pollFds.push_back(p);
}

void MulticlientTCPTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    assert(receiverId < connections.size());
    tcp::write(connections[receiverId], data, size);
}

size_t MulticlientTCPTransportServer::receive(void *whereTo, size_t maxSize) {
    const auto readable = [&] {
        for (;;) {
            const auto ret = ::poll(pollFds.data(), pollFds.size(), 5 * 1000); // 5 seconds timeout
            if (ret < 0) {
                throw std::runtime_error("Could not poll sockets: "s + ::strerror(errno));
            }
            const auto &res = std::find_if(pollFds.begin(), pollFds.end(), [](const pollfd &pollFd) {
                // check, that only POLLIN flag is set
                return ((pollFd.revents & ~POLLIN) == 0 &&
                        pollFd.revents & POLLIN) != 0;
            });

            if (res != pollFds.end()) {
                return res;
            }
        }
    }();
    ::recv(readable->fd, whereTo, maxSize, 0);
    return static_cast<size_t>(std::distance(pollFds.begin(), readable));
}

MulticlientTCPTransportClient::MulticlientTCPTransportClient() : socket(Socket::create()) {

}

MulticlientTCPTransportClient::~MulticlientTCPTransportClient() = default;

void MulticlientTCPTransportClient::connect(const std::string &ip, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    tcp::connect(socket, addr);
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
    tcp::write(socket, data, size);
}

void MulticlientTCPTransportClient::receive(void *whereTo, size_t maxSize) {
    return tcp::read(socket, whereTo, maxSize);
}
} // namespace transport
} // namespace l5
