#pragma once

#include "datastructures/VirtualRingBuffer.h"
#include "util/socket/domain.h"
#include "Transport.h"

namespace l5 {
namespace transport {

/**
 * A Shared Memory connection, the server side accepting connections on the given domain socket
 * @tparam BUFFER_SIZE the internally used buffer size. Needs to be a multiple of your used pagesize (usually 4KB)
 *         for best performance, this buffer should also fit completely into the last-level / L3 cache
 */
template<size_t BUFFER_SIZE = 16 * 1024 * 1024>
class SharedMemoryTransportServer : public TransportServer<SharedMemoryTransportServer<BUFFER_SIZE>> {
   util::Socket initialSocket;
   std::string file;
   util::Socket communicationSocket;
   std::unique_ptr<datastructure::VirtualRingBuffer> messageBuffer;

   public:
   static constexpr auto buffer_size = BUFFER_SIZE;
   /**
    * Exchange information about the shared memory via the given domain socket
    * @param domainSocket filename of the domain socket
    */
   explicit SharedMemoryTransportServer(std::string domainSocket);

   ~SharedMemoryTransportServer() override = default;

   void accept_impl();

   void write_impl(const uint8_t* data, size_t size);

   void read_impl(uint8_t* buffer, size_t size);

   size_t readSome_impl(uint8_t *buffer, size_t maxSize);
};

template<size_t BUFFER_SIZE = 16 * 1024 * 1024>
class SharedMemoryTransportClient : public TransportClient<SharedMemoryTransportClient<BUFFER_SIZE>> {
   util::Socket socket;
   std::unique_ptr<datastructure::VirtualRingBuffer> messageBuffer;

   public:
   static constexpr auto buffer_size = BUFFER_SIZE;
   SharedMemoryTransportClient() : socket(util::domain::socket()) {};

   ~SharedMemoryTransportClient() override = default;

   void connect_impl(const std::string &file);

   void reset_impl();

   void write_impl(const uint8_t* data, size_t size);

   void read_impl(uint8_t* buffer, size_t size);

   size_t readSome_impl(uint8_t *buffer, size_t maxSize);
};

template<size_t BUFFER_SIZE>
SharedMemoryTransportServer<BUFFER_SIZE>::SharedMemoryTransportServer(std::string domainSocket) :
      initialSocket(util::domain::socket()),
      file(std::move(domainSocket)) {
   util::domain::bind(initialSocket, file);
   util::domain::listen(initialSocket);
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportServer<BUFFER_SIZE>::accept_impl() {
   communicationSocket = util::domain::accept(initialSocket);

   messageBuffer = std::make_unique<datastructure::VirtualRingBuffer>(BUFFER_SIZE, communicationSocket);
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportServer<BUFFER_SIZE>::write_impl(const uint8_t* data, size_t size) {
   for (size_t i = 0; i < size;) {
      auto chunk = std::min(size - i, BUFFER_SIZE);
      messageBuffer->send(&data[i], chunk);
      i += chunk;
   }
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportServer<BUFFER_SIZE>::read_impl(uint8_t* buffer, size_t size) {
   for (size_t i = 0; i < size;) {
      auto chunk = std::min(size - i, BUFFER_SIZE);
      messageBuffer->receive(&buffer[i], chunk);
      i += chunk;
   }
}

template<size_t BUFFER_SIZE>
size_t SharedMemoryTransportServer<BUFFER_SIZE>::readSome_impl(uint8_t* buffer, size_t size) {
   auto chunk = std::min(size, BUFFER_SIZE);
   return messageBuffer->receiveSome(buffer, chunk);
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportClient<BUFFER_SIZE>::connect_impl(const std::string &file) {
   const auto pos = file.find(':');
   const auto whereTo = std::string(file.begin() + pos + 1, file.end());
   util::domain::connect(socket, whereTo);
   util::domain::unlink(whereTo);

   messageBuffer = std::make_unique<datastructure::VirtualRingBuffer>(BUFFER_SIZE, socket);
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportClient<BUFFER_SIZE>::write_impl(const uint8_t* data, size_t size) {
   for (size_t i = 0; i < size;) {
      auto chunk = std::min(size - i, BUFFER_SIZE);
      messageBuffer->send(&data[i], chunk);
      i += chunk;
   }
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportClient<BUFFER_SIZE>::read_impl(uint8_t* buffer, size_t size) {
   for (size_t i = 0; i < size;) {
      auto chunk = std::min(size - i, BUFFER_SIZE);
      messageBuffer->receive(&buffer[i], chunk);
      i += chunk;
   }
}

template<size_t BUFFER_SIZE>
size_t SharedMemoryTransportClient<BUFFER_SIZE>::readSome_impl(uint8_t* buffer, size_t size) {
   auto chunk = std::min(size, BUFFER_SIZE);
   return messageBuffer->receiveSome(buffer, chunk);
}

template<size_t BUFFER_SIZE>
void SharedMemoryTransportClient<BUFFER_SIZE>::reset_impl() {
   socket = util::domain::socket();
   messageBuffer.reset();
}

} // namespace transport
} // namespace l5
