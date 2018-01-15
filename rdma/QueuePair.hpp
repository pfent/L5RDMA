#pragma once

#include <memory>
#include <libibverbscpp/libibverbscpp.h>

namespace rdma {
    struct Address;

    class Network;
    class CompletionQueuePair;

    class QueuePair {
        std::unique_ptr<ibv::queuepair::QueuePair> qp;

        Network &network;

        CompletionQueuePair &completionQueuePair;

    public:
        QueuePair(Network &network); // Uses shared completion and receive Queue
        QueuePair(Network &network, ibv::srq::SharedReceiveQueue &receiveQueue); // Uses shared completion Queue
        QueuePair(Network &network, CompletionQueuePair &completionQueuePair); // Uses shared receive Queue
        QueuePair(Network &network, CompletionQueuePair &completionQueuePair, ibv::srq::SharedReceiveQueue &receiveQueue);

        uint32_t getQPN();

        void connect(const Address &address, unsigned retryCount = 0);

        void postWorkRequest(ibv::workrequest::SendWr &workRequest);

        uint32_t getMaxInlineSize() const;

        /// Print detailed information about this queue pair
        void printQueuePairDetails() const;

        CompletionQueuePair &getCompletionQueuePair() { return completionQueuePair; }
    };
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
