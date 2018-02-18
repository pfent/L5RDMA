#include "CompletionQueuePair.hpp"
#include <cstring>
#include <iomanip>
#include "NetworkException.h"

using namespace std;
namespace rdma {
    CompletionQueuePair::CompletionQueuePair(ibv::context::Context &ctx) :
            channel(ctx.createCompletionEventChannel()), // Create event channel
            // Create completion queues
            sendQueue(ctx.createCompletionQueue(CQ_SIZE, contextPtr, *channel, completionVector)),
            receiveQueue(ctx.createCompletionQueue(CQ_SIZE, contextPtr, *channel, completionVector)) {

        // Request notifications
        sendQueue->requestNotify(false);
        receiveQueue->requestNotify(false);
    }

    CompletionQueuePair::~CompletionQueuePair() {
        for (auto event : eventsToAck) {
            event->ackEvents(1);
        }
    }

    /// Poll a completion queue
    uint64_t CompletionQueuePair::pollCompletionQueue(ibv::completions::CompletionQueue &completionQueue,
                                                      ibv::workcompletion::Opcode type) {
        // Poll for a work completion
        ibv::workcompletion::WorkCompletion completion;
        if (completionQueue.poll(1, &completion) == 0) {
            return numeric_limits<uint64_t>::max();
        }

        // Check status and opcode
        if (not completion) {
            throw NetworkException("unexpected completion status: " + to_string(completion.getStatus()));
        }
        if (completion.getOpcode() != type) {
            throw NetworkException("unexpected completion opcode: " + to_string(completion.getOpcode()));
        }
        return completion.getId();
    }

    /// Poll the send completion queue
    uint64_t CompletionQueuePair::pollSendCompletionQueue() {
        // Poll for a work completion
        ibv::workcompletion::WorkCompletion completion;
        if (sendQueue->poll(1, &completion) == 0) {
            return numeric_limits<uint64_t>::max();
        }

        // Check status and opcode
        if (not completion) {
            throw NetworkException("unexpected completion status: " + to_string(completion.getStatus()));
        }
        return completion.getId();
    }

    uint64_t CompletionQueuePair::pollSendCompletionQueue(ibv::workcompletion::Opcode type) {
        return pollCompletionQueue(*sendQueue, type);
    }

    /// Poll the receive completion queue
    uint64_t CompletionQueuePair::pollRecvCompletionQueue() {
        return pollCompletionQueue(*receiveQueue, ibv::workcompletion::Opcode::RECV);
    }

    /// Poll a completion queue blocking
    uint64_t
    CompletionQueuePair::pollCompletionQueueBlocking(ibv::completions::CompletionQueue &completionQueue,
                                                     ibv::workcompletion::Opcode type) {
        // Poll for a work completion
        ibv::workcompletion::WorkCompletion completion;
        while (completionQueue.poll(1, &completion) == 0); // busy poll

        // Check status and opcode
        if (not completion) {
            throw NetworkException("unexpected completion status: " + to_string(completion.getStatus()));
        }
        if (completion.getOpcode() != type) {
            throw NetworkException("unexpected completion opcode: " + to_string(completion.getOpcode()));
        }
        return completion.getId();
    }

    /// Poll the send completion queue blocking
    uint64_t CompletionQueuePair::pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode opcode) {
        return pollCompletionQueueBlocking(*sendQueue, opcode);
    }

    /// Poll the receive completion queue blocking
    uint64_t CompletionQueuePair::pollRecvCompletionQueueBlocking(ibv::workcompletion::Opcode opcode) {
        return pollCompletionQueueBlocking(*receiveQueue, opcode);
    }

    /// Wait for a work completion
    void CompletionQueuePair::waitForCompletion() {
        // Wait for completion queue event
        auto[event, ctx] = channel->getEvent();
        std::ignore = ctx;

        eventsToAck.push_back(event);

        // Request a completion queue event
        event->requestNotify(false);

        // Poll all work completions
        ibv::workcompletion::WorkCompletion completion;
        for (;;) {
            auto numPolled = event->poll(1, &completion);

            if (numPolled == 0) {
                break;
            }
            if (not completion.isSuccessful()) {
                throw NetworkException("unexpected completion status: " + to_string(completion.getStatus()));
            }
        };
    }

    ibv::completions::CompletionQueue &CompletionQueuePair::getSendQueue() {
        return *sendQueue;
    }

    ibv::completions::CompletionQueue &CompletionQueuePair::getReceiveQueue() {
        return *receiveQueue;
    }

    static ibv::workcompletion::WorkCompletion pollQueueBlocking(ibv::completions::CompletionQueue &queue) {
        ibv::workcompletion::WorkCompletion completion;
        while (queue.poll(1, &completion) == 0); // busy poll
        if (not completion) {
            throw NetworkException("unexpected completion status: " + to_string(completion.getStatus()));
        }
        return completion;
    }

    ibv::workcompletion::WorkCompletion CompletionQueuePair::pollSendWorkCompletionBlocking() {
        return pollQueueBlocking(*sendQueue);
    }

    ibv::workcompletion::WorkCompletion CompletionQueuePair::pollRecvWorkCompletionBlocking() {
        return pollQueueBlocking(*receiveQueue);
    }
} // End of namespace rdma
