#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <util/socket/Socket.h>
#include <datastructures/VirtualRDMARingBuffer.h>
#include <util/socket/tcp.h>
#include "Transport.h"

namespace l5 {
namespace transport {
template<size_t BUFFER_SIZE = 16 * 1024 * 1024>
class RdmaTransportServer : public TransportServer<RdmaTransportServer<BUFFER_SIZE>> {
   const util::Socket sock;
   std::unique_ptr<datastructure::VirtualRDMARingBuffer> rdma = nullptr;

   void listen(uint16_t port);

   public:
   static constexpr auto buffer_size = BUFFER_SIZE;

   explicit RdmaTransportServer(const std::string &port);

   ~RdmaTransportServer() override = default;

   void accept_impl();

   void write_impl(const uint8_t* data, size_t size);

   void read_impl(uint8_t* buffer, size_t size);

   template<typename RangeConsumer>
   void readZC(RangeConsumer &&callback) {
      rdma->receive(std::forward<RangeConsumer>(callback));
   }

   template<typename SizeReturner>
   void writeZC(SizeReturner &&doWork) {
      rdma->send(std::forward<SizeReturner>(doWork));
   }
};

template<size_t BUFFER_SIZE = 16 * 1024 * 1024>
class RdmaTransportClient : public TransportClient<RdmaTransportClient<BUFFER_SIZE>> {
   util::Socket sock;
   std::unique_ptr<datastructure::VirtualRDMARingBuffer> rdma = nullptr;

   public:
   static constexpr auto buffer_size = BUFFER_SIZE;

   RdmaTransportClient() : sock(util::Socket::create()) {};

   ~RdmaTransportClient() override = default;

   void connect_impl(const std::string &connection);

   void reset_impl();

   void write_impl(const uint8_t* data, size_t size);

   void read_impl(uint8_t* buffer, size_t size);

   template<typename RangeConsumer>
   void readZC(RangeConsumer &&callback) {
      rdma->receive(std::forward<RangeConsumer>(callback));
   }

   template<typename SizeReturner>
   void writeZC(SizeReturner &&doWork) {
      rdma->send(std::forward<SizeReturner>(doWork));
   }
};

template<size_t BUFFER_SIZE>
RdmaTransportServer<BUFFER_SIZE>::RdmaTransportServer(const std::string &port) :
      sock(util::Socket::create()) {
   auto p = std::stoi(port);
   listen(p);
}

template<size_t BUFFER_SIZE>
void RdmaTransportServer<BUFFER_SIZE>::accept_impl() {
   auto acced = util::tcp::accept(sock);
   rdma = std::make_unique<datastructure::VirtualRDMARingBuffer>(BUFFER_SIZE, acced);
}

template<size_t BUFFER_SIZE>
void RdmaTransportServer<BUFFER_SIZE>::listen(uint16_t port) {
   util::tcp::bind(sock, port);
   util::tcp::listen(sock);
}

template<size_t BUFFER_SIZE>
void RdmaTransportServer<BUFFER_SIZE>::write_impl(const uint8_t* data, size_t size) {
   rdma->send(data, size); // TODO: chunked read / write
}

template<size_t BUFFER_SIZE>
void RdmaTransportServer<BUFFER_SIZE>::read_impl(uint8_t* buffer, size_t size) {
   rdma->receive(buffer, size);
}

template<size_t BUFFER_SIZE>
void RdmaTransportClient<BUFFER_SIZE>::connect_impl(const std::string &connection) {
   const auto pos = connection.find(':');
   if (pos == std::string::npos) {
      throw std::runtime_error("usage: <0.0.0.0:port>");
   }
   const auto ip = std::string(connection.data(), pos);
   const auto port = std::stoi(std::string(connection.begin() + pos + 1, connection.end()));

   util::tcp::connect(sock, ip, port);
   rdma = std::make_unique<datastructure::VirtualRDMARingBuffer>(BUFFER_SIZE, sock);
}

template<size_t BUFFER_SIZE>
void RdmaTransportClient<BUFFER_SIZE>::write_impl(const uint8_t* data, size_t size) {
   rdma->send(data, size); // TODO: chunked read / write
}

template<size_t BUFFER_SIZE>
void RdmaTransportClient<BUFFER_SIZE>::read_impl(uint8_t* buffer, size_t size) {
   rdma->receive(buffer, size);
}

template<size_t BUFFER_SIZE>
void RdmaTransportClient<BUFFER_SIZE>::reset_impl() {
   sock.close();
   rdma.reset();
}
} // namespace transport
} // namespace l5
