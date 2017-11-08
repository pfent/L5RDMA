#include "domainSocketsWrapper.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


int domain_socket() {
    auto sock = socket(AF_UNIX, SOCK_STREAM, 0);
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

void domain_listen(int sock) {
    if (listen(sock, SOMAXCONN) < 0) {
        perror("listen");
        throw std::runtime_error{"error close'ing"};
    }
}

void domain_connect(int sock, std::string_view pathToFile) {
    sockaddr_un local{};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
    local.sun_path[pathToFile.size()] = '\0';
    if (connect(sock, reinterpret_cast<const sockaddr *>(&local), sizeof local) < 0) {
        perror("connect");
        throw std::runtime_error{"error connect'ing"};
    }
}

void domain_write(int sock, const void *buffer, std::size_t size) {
    if (send(sock, buffer, size, 0) < 0) {
        perror("send");
        throw std::runtime_error{"error write'ing"};
    }
}

void domain_read(int sock, void *buffer, std::size_t size) {
    if (recv(sock, buffer, size, 0) < 0) {
        perror("recv");
        throw std::runtime_error{"error read'ing"};
    }
}

void domain_bind(int sock, std::string_view pathToFile) {
    // c.f. http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
    sockaddr_un local{};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
    local.sun_path[pathToFile.size()] = '\0';
    auto len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(sock, reinterpret_cast<const sockaddr *>(&local), len) < 0) {
        perror("bind");
        throw std::runtime_error{"error bind'ing"};
    }
}

void domain_unlink(std::string_view pathToFile) {
    if(unlink(std::string(pathToFile.begin(), pathToFile.end()).c_str()) < 0) {
        perror("unlink");
        throw std::runtime_error{"error unlink'ing"};
    }
}

int domain_accept(int sock, sockaddr_un &unAddr) {
    socklen_t unAddrLen = sizeof unAddr;
    auto acced = accept(sock, reinterpret_cast<sockaddr *>(&unAddr), &unAddrLen);
    if (acced < 0) {
        perror("accept");
        throw std::runtime_error{"error accept'ing"};
    }
    return acced;
}

void domain_close(int sock) {
    if (close(sock) < 0) {
        perror("close");
        throw std::runtime_error{"error close'ing"};
    }
}
