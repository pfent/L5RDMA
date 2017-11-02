#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include "tcpWrapper.h"

int tcp_socket() {
    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        throw std::runtime_error{"Could not open socket"};
    }
    const int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        throw std::runtime_error{"Could not set SO_REUSEADDR"};
    }
    return sock;
}

void tcp_connect(int sock, const sockaddr_in &addr) {
    if (connect(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) < 0) {
        perror("connect");
        throw std::runtime_error{"error connect'ing"};
    }
}

void tcp_write(int sock, const void *buffer, std::size_t size) {
    if (send(sock, buffer, size, 0) < 0) {
        perror("send");
        throw std::runtime_error{"error write'ing"};
    }
}

void tcp_read(int sock, void *buffer, std::size_t size) {
    if (recv(sock, buffer, size, 0) < 0) {
        perror("recv");
        throw std::runtime_error{"error read'ing"};
    }
}

void tcp_bind(int sock, const sockaddr_in &addr) {
    if (bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof addr) < 0) {
        perror("bind");
        throw std::runtime_error{"error bind'ing"};
    }
}

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, reinterpret_cast<sockaddr *>(&inAddr), &inAddrLen);
    if (acced < 0) {
        perror("accept");
        throw std::runtime_error{"error accept'ing"};
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
        perror("close");
        throw std::runtime_error{"error close'ing"};
    }
}

void tcp_listen(int sock) {
    if (listen(sock, SOMAXCONN) < 0) {
        perror("listen");
        throw std::runtime_error{"error close'ing"};
    }
}
