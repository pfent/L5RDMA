#pragma once

#include <emmintrin.h>
#include <util/socket/Socket.h>
#include <rdma/CompletionQueuePair.hpp>
#include <rdma/Network.hpp>
#include <rdma/MemoryRegion.h>
#include <rdma/RcQueuePair.h>

namespace l5::transport {
class MulticlientRDMADistinctMrTransportServer {
    /// State for each connection
    struct Connection {
        /// Socket from accept (currently unused after bootstrapping)
        util::Socket socket;
        /// RDMA Queue Pair
        rdma::RcQueuePair qp;
        /// The pre-prepared answer work request. Only the local data source changes for each answer
        ibv::workrequest::Simple<ibv::workrequest::Write> answerWr;
        /// Send counter to keep track when we need to signal
        size_t sendCounter = 0;
        /// Constructor
        Connection(util::Socket socket, rdma::RcQueuePair qp, ibv::workrequest::Simple<ibv::workrequest::Write> answerWr)
            : socket(std::move(socket)), qp(std::move(qp)), answerWr(answerWr){}
    };

    static constexpr size_t MAX_MESSAGESIZE = 256 * 1024 * 1024;
    static constexpr char validity = '\4'; // ASCII EOT char
    size_t MAX_CLIENTS;

    util::Socket listenSock;
    rdma::Network net;
    rdma::CompletionQueuePair *sharedCq;

    rdma::RegisteredMemoryRegion<uint8_t[MAX_MESSAGESIZE]> receives;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    size_t sendCounter = 0;

    std::vector<Connection> connections;

    void listen(uint16_t port);

    template <class T>
    static constexpr auto setWrFlags(T& wr, bool signaled, bool inlineMsg) {
        if (signaled && inlineMsg) return wr.setFlags({ibv::workrequest::Flags::SIGNALED, ibv::workrequest::Flags::INLINE});
        if (signaled) return wr.setFlags({ibv::workrequest::Flags::SIGNALED});
        if (inlineMsg) return wr.setFlags({ibv::workrequest::Flags::INLINE});
        return wr.setFlags({});
    }

public:
    explicit MulticlientRDMADistinctMrTransportServer(const std::string &port, size_t maxClients = 256);

    ~MulticlientRDMADistinctMrTransportServer() = default;

    MulticlientRDMADistinctMrTransportServer(MulticlientRDMADistinctMrTransportServer &&) = default;

    MulticlientRDMADistinctMrTransportServer &operator=(MulticlientRDMADistinctMrTransportServer &&) = default;

    void accept();

    void finishListen();

    /// polls all possible clients for incoming messages and copys the first one it finds to "whereTo"
    size_t receive(void *whereTo, size_t maxSize);

    void send(size_t receiverId, const uint8_t *data, size_t size);

    template<typename TriviallyCopyable>
    void write(size_t receiverId, const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        send(receiverId, reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    template<typename TriviallyCopyable>
    size_t read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        return receive(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }
};

class MulticlientRDMADistinctMrTransportClient {
    static constexpr size_t MAX_MESSAGESIZE = 256 * 1024 * 1024;
    static constexpr char validity = '\4'; // ASCII EOT char

    util::Socket sock;
    rdma::Network net;
    rdma::CompletionQueuePair &cq;
    rdma::RcQueuePair qp;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    rdma::RegisteredMemoryRegion<uint8_t> receiveBuffer;

    ibv::workrequest::Simple<ibv::workrequest::Write> dataWr;

    void rdmaConnect();

public:
    MulticlientRDMADistinctMrTransportClient();

    void connect(std::string_view whereTo);

    void connect(const std::string &ip, uint16_t port);

    void send(const uint8_t *data, size_t size);

    size_t receive(void *whereTo, size_t maxSize);

    template<typename TriviallyCopyable>
    void write(const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        send(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    template<typename TriviallyCopyable>
    void read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        receive(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }
};
} // namespace l5
