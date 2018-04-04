#ifndef EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H
#define EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H

#include <rdma/RcQueuePair.h>
#include "rdma/Network.hpp"
#include "rdma/MemoryRegion.h"

class MulticlientTransportServer {
    struct Connection {
        int socket;
        rdma::RcQueuePair qp;
        ibv::workrequest::Simple<ibv::workrequest::Write> answerWr;
    };

    static constexpr size_t MAX_MESSAGESIZE = 512;
    static constexpr size_t MAX_CLIENTS = 256;
    static constexpr char validity = '\4'; // ASCII EOT char

    const int listenSock;
    rdma::Network net;
    rdma::CompletionQueuePair &sharedCq;

    rdma::RegisteredMemoryRegion<uint8_t[MAX_MESSAGESIZE]> receives;

    rdma::RegisteredMemoryRegion<char> doorBells;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    size_t sendCounter = 0;

    std::vector<Connection> connections;

    void listen(uint16_t port);

    __always_inline
    static size_t poll(char *doorBells, size_t count) noexcept {
        for (;;) {
            for (size_t i = 0; i < count; ++i) {
                if (*reinterpret_cast<volatile char *>(&doorBells[i]) != '\0') {
                    doorBells[i] = '\0';
                    return i;
                }
            }
        }
    }

public:
    explicit MulticlientTransportServer(std::string_view port);

    ~MulticlientTransportServer();

    void accept();

    /// polls all possible clients for incoming messages and copys the first one it finds to "whereTo"
    size_t receive(void *whereTo, size_t maxSize);

    void send(size_t receiverId, const uint8_t *data, size_t size);

    /// send data via a lambda to enable zerocopy operation
    /// expected signature: [](uint8_t* begin) -> size_t
    template<typename SizeReturner>
    void send(size_t receiverId, SizeReturner &&doWork) {
        if (receiverId > connections.size()) {
            throw std::runtime_error("no such connection");
        }

        auto &con = connections[receiverId];

        auto sizePtr = reinterpret_cast<size_t *>(sendBuffer.data());
        auto begin = sendBuffer.data() + sizeof(size_t);

        const auto size = doWork(begin);
        const auto totalLength = size + sizeof(size_t) + sizeof(validity);
        if (totalLength > MAX_MESSAGESIZE) {
            throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
        }

        auto validityPtr = sendBuffer.data() + sizeof(size_t) + size;

        *sizePtr = size;
        *validityPtr = validity;

        con.answerWr.setLocalAddress(sendBuffer.getSlice(0, totalLength));
        sendCounter++;
        if (sendCounter % 1024 == 0) { // selective signaling
            con.answerWr.setFlags({ibv::workrequest::Flags::INLINE, ibv::workrequest::Flags::SIGNALED});
            con.qp.postWorkRequest(con.answerWr);
            sharedCq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        } else {
            con.answerWr.setFlags({ibv::workrequest::Flags::INLINE});
            con.qp.postWorkRequest(con.answerWr);
        }
    }

    /// receive data via a lambda to enable zerocopy operation
    /// expected signature: [](size_t sender, const uint8_t* begin, const uint8_t* end) -> void
    template<typename RangeConsumer>
    void receive(RangeConsumer &&callback) {
        const auto sender = poll(doorBells.data(), MAX_CLIENTS);

        const auto sizePtr = reinterpret_cast<uint8_t *>(receives.data()[sender]);
        const auto size = *reinterpret_cast<size_t *>(sizePtr);

        const auto begin = sizePtr + sizeof(size_t);
        const auto end = begin + size;
        callback(sender, begin, end);
    }
};

class MultiClientTransportClient {
    static constexpr size_t MAX_MESSAGESIZE = 512;
    static constexpr char validity = '\4'; // ASCII EOT char

    const int sock;
    rdma::Network net;
    rdma::CompletionQueuePair &cq;
    rdma::RcQueuePair qp;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    rdma::RegisteredMemoryRegion<char> doorBell;
    rdma::RegisteredMemoryRegion<uint8_t> receiveBuffer;

    ibv::workrequest::Simple<ibv::workrequest::Write> dataWr;
    ibv::workrequest::Simple<ibv::workrequest::Write> doorBellWr;

    void rdmaConnect();

public:
    MultiClientTransportClient();

    void connect(std::string_view whereTo);

    void connect(std::string_view ip, uint16_t port);

    void send(const uint8_t *data, size_t size);

    size_t receive(void *whereTo, size_t maxSize);

    /// send data via a lambda to enable zerocopy operation
    /// expected signature: [](uint8_t* begin) -> size_t
    template<typename SizeReturner>
    void send(SizeReturner &&doWork) {
        auto sizePtr = reinterpret_cast<size_t *>(sendBuffer.data());
        auto begin = sendBuffer.data() + sizeof(size_t);

        const auto size = doWork(begin);
        const auto dataWrSize = size + sizeof(size_t);
        if (dataWrSize > MAX_MESSAGESIZE) {
            throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
        }

        *sizePtr = size;

        dataWr.setLocalAddress(sendBuffer.getSlice(0, dataWrSize));
        qp.postWorkRequest(dataWr);

        doorBell.data()[0] = 'X'; // could be anything, really
        qp.postWorkRequest(doorBellWr);

        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }

    /// receive data via a lambda to enable zerocopy operation
    /// expected signature: [](const uint8_t* begin, const uint8_t* end) -> void
    template<typename RangeConsumer>
    void receive(RangeConsumer &&callback) {
        size_t size;
        while ((size = *reinterpret_cast<volatile size_t *>(receiveBuffer.data())) == 0);
        while (*reinterpret_cast<volatile char *>(receiveBuffer.data() + sizeof(size_t) + size) != validity);

        const auto begin = receiveBuffer.data() + sizeof(size_t);
        const auto end = begin + size;

        callback(begin, end);
        *reinterpret_cast<volatile size_t *>(receiveBuffer.data()) = 0;
    }
};

#endif //EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H
