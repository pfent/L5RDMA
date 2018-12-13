#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include <libibverbscpp.h>

namespace rdma {
    class CompletionQueuePair {
        static constexpr void *contextPtr = nullptr;
        static constexpr int completionVector = 0;
        /// The minimal number of entries for the completion queue
        static constexpr int CQ_SIZE = 100;

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

        uint64_t
        pollCompletionQueue(ibv::completions::CompletionQueue &completionQueue, ibv::workcompletion::Opcode type);

        std::vector<ibv::completions::CompletionQueue *> eventsToAck;

    public:
        explicit CompletionQueuePair(ibv::context::Context &ctx);

        ~CompletionQueuePair();

        ibv::completions::CompletionQueue &getSendQueue();

        ibv::completions::CompletionQueue &getReceiveQueue();

        /// Poll the send completion queue
        uint64_t pollSendCompletionQueue();

        /// Poll the send completion queue with a user defined type
        uint64_t pollSendCompletionQueue(ibv::workcompletion::Opcode type);

        /// Poll the receive completion queue
        uint64_t pollRecvCompletionQueue();

        // Poll a completion queue blocking
        uint64_t pollCompletionQueueBlocking(ibv::completions::CompletionQueue &completionQueue,
                                             ibv::workcompletion::Opcode type);

        /// Poll the send completion queue blocking
        uint64_t
        pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode opcode = ibv::workcompletion::Opcode::RDMA_READ);

        /// Poll the receive completion queue blocking
        uint64_t pollRecvCompletionQueueBlocking(ibv::workcompletion::Opcode opcode = ibv::workcompletion::Opcode::RECV);

        ibv::workcompletion::WorkCompletion pollSendWorkCompletionBlocking();

        ibv::workcompletion::WorkCompletion pollRecvWorkCompletionBlocking();

        /// Wait for a work request completion
        void waitForCompletion();
    };
} // End of namespace rdma
