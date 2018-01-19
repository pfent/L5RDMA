#pragma once

#include <memory>
#include <libibverbscpp/libibverbscpp.h>
#include "Network.hpp"

namespace rdma {
    struct Address;

    class Network;

    class CompletionQueuePair;

    class QueuePair {
        static constexpr void *context = nullptr; // Associated context of the QP (returned in completion events)
        static constexpr uint32_t maxOutstandingSendWrs = 16351; // max number of outstanding WRs in the SQ
        static constexpr uint32_t maxOutstandingRecvWrs = 16351; // max number of outstanding WRs in the RQ
        static constexpr uint32_t maxSlicesPerSendWr = 1; // max number of scatter/gather elements in a WR in the SQ
        static constexpr uint32_t maxSlicesPerRecvWr = 1; // max number of scatter/gather elements in a WR in the RQ
        static constexpr uint32_t maxInlineSize = 512; // max number of bytes that can be posted inline to the SQ
        static constexpr auto signalAll = false; // If each Work Request (WR) submitted to the SQ generates a completion entry

        std::unique_ptr<ibv::queuepair::QueuePair> qp;

        Network &network; // TODO: decouple

        const ibv::queuepair::Type type;

        void connectRC(const Address &address, uint8_t port, uint8_t retryCount = 0);

        void connectUD(const Address &address, uint8_t port, uint32_t packetSequenceNumber = 0);

    public:
        // Uses shared completion and receive Queue
        QueuePair(Network &network, ibv::queuepair::Type type);

        // Uses shared completion Queue
        QueuePair(Network &network, ibv::queuepair::Type type, ibv::srq::SharedReceiveQueue &receiveQueue);

        // Uses shared receive Queue
        QueuePair(Network &network, ibv::queuepair::Type type, CompletionQueuePair &completionQueuePair);

        QueuePair(Network &network, ibv::queuepair::Type type, CompletionQueuePair &completionQueuePair,
                  ibv::srq::SharedReceiveQueue &receiveQueue);

        uint32_t getQPN();

        void connect(const Address &address);

        void postWorkRequest(ibv::workrequest::SendWr &workRequest);

        void postRecvRequest(ibv::workrequest::Recv &recvRequest);

        uint32_t getMaxInlineSize() const;

        /// Print detailed information about this queue pair
        void printQueuePairDetails() const;
    };
} // End of namespace rdma
