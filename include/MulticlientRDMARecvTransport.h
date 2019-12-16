#pragma once

#include "rdma/CompletionQueuePair.hpp"
#include "rdma/MemoryRegion.h"
#include "rdma/Network.hpp"
#include "rdma/RcQueuePair.h"
#include "util/socket/Socket.h"
#include <unordered_map>
#include <emmintrin.h>

namespace l5::transport {
class MulticlientRDMARecvTransportServer {
   /// State for each connection
   struct Connection {
      /// Socket from accept (currently unused after bootstrapping)
      util::Socket socket;
      /// RDMA Queue Pair
      rdma::RcQueuePair qp;
      /// The pre-prepared answer work request. Only the local data source changes for each answer
      ibv::workrequest::Simple<ibv::workrequest::Write> answerWr;
      /// The receive request for incoming messages
      ibv::workrequest::Recv recv;
      /// Send counter to keep track when we need to signal
      size_t sendCounter = 0; // TODO: instead of a send counter, maybe use an "unsignaled" count with a threshold
      /// Constructor
      Connection(util::Socket socket, rdma::RcQueuePair qp, ibv::workrequest::Simple<ibv::workrequest::Write> answerWr,
                 ibv::workrequest::Recv recv)
         : socket(std::move(socket)), qp(std::move(qp)), answerWr(answerWr), recv(recv) {}
   };

   /// Maximum supported message size in byte
   static constexpr size_t MAX_MESSAGESIZE = 256 * 1024;
   /// The OK byte used to detect partially written messages
   static constexpr char validity = '\4'; // ASCII EOT char
   /// How many clients can concurrently connect
   size_t MAX_CLIENTS;

   util::Socket listenSock;
   rdma::Network net;
   rdma::CompletionQueuePair* sharedCq;
   rdma::RegisteredMemoryRegion<uint8_t[MAX_MESSAGESIZE]> receives;
   rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
   std::vector<Connection> connections;
   std::unordered_map<uint32_t, uint32_t> qpnToConnection;

   void listen(uint16_t port);

   template <class T>
   static constexpr auto setWrFlags(T& wr, bool signaled, bool inlineMsg) {
      if (signaled && inlineMsg) return wr.setFlags({ibv::workrequest::Flags::SIGNALED, ibv::workrequest::Flags::INLINE});
      if (signaled) return wr.setFlags({ibv::workrequest::Flags::SIGNALED});
      if (inlineMsg) return wr.setFlags({ibv::workrequest::Flags::INLINE});
      return wr.setFlags({});
   }

   public:
   explicit MulticlientRDMARecvTransportServer(const std::string& port, size_t maxClients = 256);

   ~MulticlientRDMARecvTransportServer() = default;

   MulticlientRDMARecvTransportServer(MulticlientRDMARecvTransportServer&&) = default;

   MulticlientRDMARecvTransportServer& operator=(MulticlientRDMARecvTransportServer&&) = default;

   void accept();

   void finishListen();

   /// polls all possible clients for incoming messages and copys the first one it finds to "whereTo"
   size_t receive(void* whereTo, size_t maxSize);

   void send(size_t receiverId, const uint8_t* data, size_t size);

   template <typename TriviallyCopyable>
   void write(size_t receiverId, const TriviallyCopyable& data) {
      static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
      send(receiverId, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
   }

   template <typename TriviallyCopyable>
   size_t read(TriviallyCopyable& data) {
      static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
      return receive(reinterpret_cast<uint8_t*>(&data), sizeof(data));
   }
};

class MulticlientRDMARecvTransportClient {
   static constexpr size_t MAX_MESSAGESIZE = 256 * 1024 * 1024;
   static constexpr char validity = '\4'; // ASCII EOT char

   util::Socket sock;
   rdma::Network net;
   rdma::CompletionQueuePair& cq;
   rdma::RcQueuePair qp;

   rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
   rdma::RegisteredMemoryRegion<uint8_t> receiveBuffer;

   /// Write with immediate, consumes a RECV on the server so we can poll using a shared completion queue
   ibv::workrequest::Simple<ibv::workrequest::WriteWithImm> dataWr;

   void rdmaConnect();

   public:
   MulticlientRDMARecvTransportClient();

   void connect(std::string_view whereTo);

   void connect(const std::string& ip, uint16_t port);

   void send(const uint8_t* data, size_t size);

   size_t receive(void* whereTo, size_t maxSize);

   template <typename TriviallyCopyable>
   void write(const TriviallyCopyable& data) {
      static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
      send(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
   }

   template <typename TriviallyCopyable>
   void read(TriviallyCopyable& data) {
      static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
      receive(reinterpret_cast<uint8_t*>(&data), sizeof(data));
   }
};
} // namespace l5
