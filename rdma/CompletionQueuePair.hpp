#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include <libibverbscpp/libibverbscpp.h>

namespace rdma {
    class Network;

    class CompletionQueuePair {
        friend class QueuePair;

        /// The completion channel
        std::unique_ptr<ibv::completions::CompletionEventChannel> channel;
        /// The send completion queue
        std::unique_ptr<ibv::completions::CompletionQueue> sendQueue;
        /// The receive completion queue
        std::unique_ptr<ibv::completions::CompletionQueue> receiveQueue;

        /// The cached work completions
        std::vector<std::pair<bool, uint64_t>> cachedCompletions;
        /// Protect wait for events method from concurrent access
        std::mutex guard;

        uint64_t pollCompletionQueue(ibv::completions::CompletionQueue& completionQueue, ibv::workcompletion::Opcode type);

        std::pair<bool, uint64_t> waitForCompletion(bool restrict, bool onlySend);

        std::vector<ibv::completions::CompletionQueue *> eventsToAck;

    public:
        /// Ctor
        explicit CompletionQueuePair(Network &network);

        ~CompletionQueuePair();

        /// Poll the send completion queue
        uint64_t pollSendCompletionQueue();

        /// Poll the send completion queue with a user defined type
        uint64_t pollSendCompletionQueue(ibv::workcompletion::Opcode type);

        /// Poll the receive completion queue
        uint64_t pollRecvCompletionQueue();

        // Poll a completion queue blocking
        uint64_t pollCompletionQueueBlocking(ibv::completions::CompletionQueue& completionQueue, ibv::workcompletion::Opcode type);

        /// Poll the send completion queue blocking
        uint64_t pollSendCompletionQueueBlocking();

        /// Poll the receive completion queue blocking
        uint64_t pollRecvCompletionQueueBlocking();

        /// Wait for a work request completion
        void waitForCompletion();

        uint64_t waitForCompletionSend();

        uint64_t waitForCompletionReceive();
    };
} // End of namespace rdma
