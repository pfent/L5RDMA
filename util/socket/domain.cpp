#include "domain.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <array>

namespace l5 {
namespace util {
namespace domain {

using namespace std::string_literals;

Socket socket() {
   return Socket::create(AF_UNIX, SOCK_STREAM, 0);
}

void listen(const Socket &sock) {
   if (::listen(sock.get(), SOMAXCONN) < 0) {
      throw std::runtime_error{"error close'ing"s + strerror(errno)};
   }
}

void connect(const Socket &sock, const std::string &pathToFile) {
   ::sockaddr_un local{};
   local.sun_family = AF_UNIX;
   strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
   local.sun_path[pathToFile.size()] = '\0';
   if (::connect(sock.get(), reinterpret_cast<const sockaddr*>(&local), sizeof local) < 0) {
      throw std::runtime_error{"error connect'ing"s + strerror(errno)};
   }
}

void write(const Socket &sock, const void* buffer, std::size_t size) {
   for (size_t current = 0; size > 0;) {
      auto res = ::send(sock.get(), reinterpret_cast<const char*>(buffer) + current, size, 0);
      if (res < 0) {
         throw std::runtime_error("Couldn't write to socket: "s + strerror(errno));
      }
      current += res;
      size -= res;
   }
}

void read(const Socket &sock, void* buffer, std::size_t size) {
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

void bind(const Socket &sock, const std::string &pathToFile) {
   // c.f. http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
   ::sockaddr_un local{};
   local.sun_family = AF_UNIX;
   strncpy(local.sun_path, pathToFile.data(), pathToFile.size());
   local.sun_path[pathToFile.size()] = '\0';
   auto len = strlen(local.sun_path) + sizeof(local.sun_family);
   if (::bind(sock.get(), reinterpret_cast<const sockaddr*>(&local), len) < 0) {
      throw std::runtime_error{"error bind'ing"s + strerror(errno)};
   }
}

void unlink(const std::string &pathToFile) {
   if (::unlink(std::string(pathToFile.begin(), pathToFile.end()).c_str()) < 0) {
      throw std::runtime_error{"error unlink'ing"s + strerror(errno)};
   }
}

Socket accept(const Socket &sock, ::sockaddr_un &inAddr) {
   socklen_t unAddrLen = sizeof(inAddr);
   auto acced = ::accept(sock.get(), reinterpret_cast<sockaddr*>(&inAddr), &unAddrLen);
   if (acced < 0) {
      throw std::runtime_error{"error accept'ing"s + strerror(errno)};
   }
   return Socket::fromRaw(acced);
}

Socket accept(const Socket &sock) {
   auto acced = ::accept(sock.get(), nullptr, nullptr);
   if (acced < 0) {
      throw std::runtime_error{"error accept'ing"s + strerror(errno)};
   }
   return Socket::fromRaw(acced);
}

void send_fd(const Socket &sock, int fd) {
   auto data = std::array<char, 1>();
   auto iov = iovec();
   iov.iov_base = data.data();
   iov.iov_len = data.size();

   auto ctrl_buf = std::array<char, CMSG_SPACE(sizeof(int))>();
   auto msg = msghdr();
   msg.msg_name = nullptr;
   msg.msg_namelen = 0;
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;
   msg.msg_control = ctrl_buf.data();
   msg.msg_controllen = ctrl_buf.size();

   auto cmsg = CMSG_FIRSTHDR(&msg);
   cmsg->cmsg_level = SOL_SOCKET;
   cmsg->cmsg_type = SCM_RIGHTS;
   cmsg->cmsg_len = CMSG_LEN(sizeof(int));

   *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = fd;

   if (::sendmsg(sock.get(), &msg, 0) < 0) {
      throw std::runtime_error("send_fd: sendmsg failed"s + strerror(errno));
   }
}

int receive_fd(const Socket &sock) {
   auto data = std::array<char, 1>();
   auto iov = iovec();
   iov.iov_base = data.data();
   iov.iov_len = data.size();

   auto ctrl_buf = std::array<char, CMSG_SPACE(sizeof(int))>();
   auto msg = msghdr();
   msg.msg_name = nullptr;
   msg.msg_namelen = 0;
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;
   msg.msg_control = ctrl_buf.data();
   msg.msg_controllen = ctrl_buf.size();

   const auto n = ::recvmsg(sock.get(), &msg, 0);
   if (n < 0) {
      throw std::runtime_error("receive_fd: recvmsg failed"s + strerror(errno));
   }
   if (n == 0) {
      throw std::runtime_error("receive_fd: invalid FD received");
   }

   const auto cmsg = CMSG_FIRSTHDR(&msg);
   if (cmsg == nullptr) {
      throw std::runtime_error("receive_fd: invalid FD received");
   }
   int fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg));
   return fd;
}
} // namespace domain
} // namespace util
} // namespace l5
