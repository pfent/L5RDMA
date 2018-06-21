#include "domain.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace l5 {
namespace util {
namespace domain {
Socket socket() {
    return Socket::create(AF_UNIX, SOCK_STREAM, 0);
}

void listen(const Socket &sock) {
    if (::listen(sock.get(), SOMAXCONN) < 0) {
        perror("listen");
        throw std::runtime_error{"error close'ing"};
    }
}

void connect(const Socket &sock, const std::string &pathToFile) {
    ::sockaddr_un local{};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
    local.sun_path[pathToFile.size()] = '\0';
    if (::connect(sock.get(), reinterpret_cast<const sockaddr *>(&local), sizeof local) < 0) {
        perror("connect");
        throw std::runtime_error{"error connect'ing"};
    }
}

void write(const Socket &sock, const void *buffer, std::size_t size) {
    if (::send(sock.get(), buffer, size, 0) < 0) {
        perror("send");
        throw std::runtime_error{"error write'ing"};
    }
}

size_t read(const Socket &sock, void *buffer, std::size_t size) {
    ssize_t res = recv(sock.get(), buffer, size, 0);
    if (res < 0) {
        perror("recv");
        throw std::runtime_error{"error read'ing"};
    }
    return static_cast<size_t>(res);
}

void bind(const Socket &sock, const std::string &pathToFile) {
    // c.f. http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
    ::sockaddr_un local{};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
    local.sun_path[pathToFile.size()] = '\0';
    auto len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (::bind(sock.get(), reinterpret_cast<const sockaddr *>(&local), len) < 0) {
        perror("bind");
        throw std::runtime_error{"error bind'ing"};
    }
}

void unlink(const std::string &pathToFile) {
    if (::unlink(std::string(pathToFile.begin(), pathToFile.end()).c_str()) < 0) {
        perror("unlink");
        throw std::runtime_error{"error unlink'ing"};
    }
}

Socket accept(const Socket &sock, ::sockaddr_un &inAddr) {
    socklen_t unAddrLen = sizeof(inAddr);
    auto acced = ::accept(sock.get(), reinterpret_cast<sockaddr *>(&inAddr), &unAddrLen);
    if (acced < 0) {
        perror("accept");
        throw std::runtime_error{"error accept'ing"};
    }
    return Socket::fromRaw(acced);
}

Socket accept(const Socket &sock) {
    auto acced = ::accept(sock.get(), nullptr, nullptr);
    if (acced < 0) {
        perror("accept");
        throw std::runtime_error{"error accept'ing"};
    }
    return Socket::fromRaw(acced);
}
} // namespace domain
} // namespace util
} // namespace l5
