#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tcp.h"

using namespace std::string_literals;

void l5::util::tcp::connect(const l5::util::Socket &sock, const sockaddr_in &dest) {
   auto addr = reinterpret_cast<const sockaddr*>(&dest);
   if (::connect(sock.get(), addr, sizeof(dest)) < 0) {
      throw std::runtime_error("Couldn't connect: "s + strerror(errno));
   }
}

void l5::util::tcp::connect(const l5::util::Socket &sock, const std::string &ip, uint16_t port) {
   sockaddr_in addr = {};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

   connect(sock, addr);
}

void l5::util::tcp::write(const l5::util::Socket &sock, const void* buffer, std::size_t size) {
   for (size_t current = 0; size > 0;) {
      auto res = ::send(sock.get(), reinterpret_cast<const char*>(buffer) + current, size, 0);
      if (res < 0) {
         throw std::runtime_error("Couldn't write to socket: "s + strerror(errno));
      }
      current += res;
      size -= res;
   }
}

void l5::util::tcp::read(const l5::util::Socket &sock, void* buffer, std::size_t size) {
   for (size_t current = 0; size > 0;) {
      auto res = ::recv(sock.get(), reinterpret_cast<char*>(buffer) + current, size, 0);
      if (res < 0) {
         throw std::runtime_error("Couldn't read from socket: "s + strerror(errno));
      }
      if (static_cast<size_t>(res) == size) {
         return;
      }
      current += res;
      size -= res;
   }
}

void l5::util::tcp::bind(const l5::util::Socket &sock, const sockaddr_in &addr) {
   auto what = reinterpret_cast<const sockaddr*>(&addr);
   if (::bind(sock.get(), what, sizeof(addr)) < 0) {
      throw std::runtime_error("Couldn't bind socket: "s + strerror(errno));
   }
}

void l5::util::tcp::bind(const l5::util::Socket &sock, uint16_t port) {
   sockaddr_in addr{};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr.s_addr = INADDR_ANY;

   bind(sock, addr);
}

void l5::util::tcp::listen(const l5::util::Socket &sock) {
   if (::listen(sock.get(), SOMAXCONN) < 0) {
      throw std::runtime_error("Couldn't listen on socket: "s + strerror(errno));
   }
}

l5::util::Socket l5::util::tcp::accept(const l5::util::Socket &sock, sockaddr_in &inAddr) {
   socklen_t inAddrLen = sizeof(inAddr);
   auto saddr = reinterpret_cast<sockaddr*>(&inAddr);
   auto acced = ::accept(sock.get(), saddr, &inAddrLen);
   if (acced < 0) {
      throw std::runtime_error("Couldn't accept from socket: "s + strerror(errno));
   }
   return Socket::fromRaw(acced);
}

l5::util::Socket l5::util::tcp::accept(const l5::util::Socket &sock) {
   auto acced = ::accept(sock.get(), nullptr, nullptr);
   if (acced < 0) {
      throw std::runtime_error("Couldn't accept from socket: "s + strerror(errno));
   }
   return Socket::fromRaw(acced);
}

void l5::util::tcp::setBlocking(const l5::util::Socket &sock) {
   auto opts = fcntl(sock.get(), F_GETFL);
   opts &= ~O_NONBLOCK;
   fcntl(sock.get(), F_SETFL, opts);
}
