#pragma once

#include <netinet/in.h>
#include <string>
#include "Socket.h"

namespace l5 {
namespace util {
namespace tcp {
void connect(const Socket &sock, const sockaddr_in &dest);

void connect(const Socket &sock, const std::string &ip, uint16_t port);

void write(const Socket &sock, const void *buffer, std::size_t size);

template<typename T>
void write(const Socket &sock, const T &object) {
    static_assert(std::is_trivially_copyable<T>::value, "");
    write(sock, reinterpret_cast<const uint8_t *>(&object), sizeof(object));
}

void read(const Socket &sock, void *buffer, std::size_t size);

template<typename T>
void read(const Socket &sock, T &object) {
    static_assert(std::is_trivially_copyable<T>::value, "");
    return read(sock, &object, sizeof(object));
}

template<typename T>
T read(const Socket &sock) {
    static_assert(std::is_trivially_constructible<T>::value, "");
    T res;
    read(sock, res);
    return res;
}

void bind(const Socket &sock, const sockaddr_in &addr);

void bind(const Socket &sock, uint16_t port);

void listen(const Socket &sock);

Socket accept(const Socket &sock, sockaddr_in &inAddr);

Socket accept(const Socket &sock);

void setBlocking(const Socket &sock);
} // namespace tcp
} // namespace util
} // namespace l5
