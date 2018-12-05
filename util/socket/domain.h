#pragma once

#include <cstddef>
#include <string>
#include <sys/un.h>
#include "Socket.h"

namespace l5 {
namespace util {
namespace domain {
struct sockaddr_un;

Socket socket();

void listen(const Socket &sock);

void connect(const Socket &sock, const std::string &pathToFile);

void write(const Socket &sock, const void* buffer, std::size_t size);

template<typename T>
void write(const Socket &sock, const T &object) {
   static_assert(std::is_trivially_copyable<T>::value, "");
   write(sock, reinterpret_cast<const uint8_t*>(&object), sizeof(object));
}

void read(const Socket &sock, void* buffer, std::size_t size);

template<typename T>
void read(const Socket &sock, T &object) {
   static_assert(std::is_trivially_copyable<T>::value, "");
   read(sock, &object, sizeof(object));
}

template<typename T>
T read(const Socket &sock) {
   static_assert(std::is_trivially_constructible<T>::value, "");
   T res;
   read(sock, res);
   return res;
}

void bind(const Socket &sock, const std::string &pathToFile);

Socket accept(const Socket &sock, ::sockaddr_un &inAddr);

Socket accept(const Socket &sock);

void setBlocking(const Socket &sock);

void unlink(const std::string &pathToFile);

/// Send a file descriptor over a domain socket
void send_fd(const Socket &sock, int fd);

/// Receive a file descriptor over a domain socket
int receive_fd(const Socket &sock);

} // namespace domain
} // namespace util
} // namespace l5
