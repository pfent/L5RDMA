#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <arpa/inet.h>
#include "tcpWrapper.h"

using namespace std::string_literals;

int tcp_socket() {
    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error{"Could not open socket: "s + strerror(errno)};
    }
    const int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        throw std::runtime_error{"Could not set SO_REUSEADDR: "s + strerror(errno)};
    }
    return sock;
}

void tcp_connect(int sock, const sockaddr_in &addr) {
    if (connect(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) < 0) {
        throw std::runtime_error{"error connect'ing: "s + strerror(errno)};
    }
}

void tcp_write(int sock, const void *buffer, std::size_t size) {
    if (send(sock, buffer, size, 0) < 0) {
        throw std::runtime_error{"error write'ing: "s + strerror(errno)};
    }
}

void tcp_read(int sock, void *buffer, std::size_t size) {
    if (recv(sock, buffer, size, 0) < 0) {
        throw std::runtime_error{"error read'ing: "s + strerror(errno)};
    }
}

void tcp_bind(int sock, const sockaddr_in &addr) {
    if (bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) < 0) {
        throw std::runtime_error{"error bind'ing: "s + strerror(errno)};
    }
}

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, reinterpret_cast<sockaddr *>(&inAddr), &inAddrLen);
    if (acced < 0) {
        throw std::runtime_error{"error accept'ing: "s + strerror(errno)};
    }
    return acced;
}

void tcp_setBlocking(int sock) {
    auto opts = fcntl(sock, F_GETFL);
    opts &= ~O_NONBLOCK;
    fcntl(sock, F_SETFL, opts);
}

void tcp_close(int sock) {
    if (close(sock) < 0) {
        throw std::runtime_error{"error close'ing: "s + strerror(errno)};
    }
}

void tcp_listen(int sock) {
    if (listen(sock, SOMAXCONN) < 0) {
        throw std::runtime_error{"error listen'ing: "s + strerror(errno)};
    }
}

void tcp_connect(int sock, std::string_view whereTo) {
    const auto pos = whereTo.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(whereTo.data(), pos);
    const auto port = std::stoi(std::string(whereTo.begin() + pos + 1, whereTo.end())); // TODO: std::from_chars?
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    tcp_connect(sock, addr);
}
