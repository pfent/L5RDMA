#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include "tcpWrapper.h"

int tcp_socket() {
    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error{"Could not open socket"};
    }
    const int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        throw std::runtime_error{"Could not set SO_REUSEADDR"};
    }
    return sock;
}

void tcp_connect(int sock, sockaddr_in &addr) {
    if (connect(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw std::runtime_error{"error connect'ing"};
    }
}

void tcp_write(int sock, void *buffer, std::size_t size) {
    if (write(sock, buffer, size) < 0) {
        throw std::runtime_error{"error write'ing"};
    }
}

void tcp_read(int sock, void *buffer, std::size_t size) {
    if (read(sock, buffer, size) < 0) {
        throw std::runtime_error{"error read'ing"};
    }
}

void tcp_bind(int sock, sockaddr_in &addr) {
    if (bind(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw std::runtime_error{"error bind'ing"};
    }
}

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, (sockaddr *) &inAddr, &inAddrLen);
    if (acced < 0) {
        throw std::runtime_error{"error accept'ing"};
    }
    return acced;
}