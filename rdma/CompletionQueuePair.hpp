//---------------------------------------------------------------------------
// (c) 2015 Wolf Roediger <roediger@in.tum.de>
// Technische Universitaet Muenchen
// Institut fuer Informatik, Lehrstuhl III
// Boltzmannstr. 3
// 85748 Garching
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include <vector>
#include <cstdint>
#include <mutex>

//---------------------------------------------------------------------------
struct ibv_comp_channel;
struct ibv_srq;
struct ibv_cq;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    class Network;

//---------------------------------------------------------------------------
    class CompletionQueuePair {
        friend class QueuePair;

        CompletionQueuePair(CompletionQueuePair const &) = delete;

        CompletionQueuePair &operator=(CompletionQueuePair const &) = delete;

        /// The send completion queue
        ibv_cq *sendQueue;
        /// The receive completion queue
        ibv_cq *receiveQueue;
        /// The completion channel
        ibv_comp_channel *channel;

        /// The cached work completions
        std::vector<std::pair<bool, uint64_t>> cachedCompletions;
        /// Protect wait for events method from concurrent access
        std::mutex guard;

        uint64_t pollCompletionQueue(ibv_cq *completionQueue, int type);

        std::pair<bool, uint64_t> waitForCompletion(bool restrict, bool onlySend);

    public:
        /// Ctor
        CompletionQueuePair(Network &network);

        ~CompletionQueuePair();

        /// Poll the send completion queue
        uint64_t pollSendCompletionQueue();

        /// Poll the send completion queue with a user defined type
        uint64_t pollSendCompletionQueue(int type);

        /// Poll the receive completion queue
        uint64_t pollRecvCompletionQueue();

        // Poll a completion queue blocking
        uint64_t pollCompletionQueueBlocking(ibv_cq *completionQueue, int type);

        /// Poll the send completion queue blocking
        uint64_t pollSendCompletionQueueBlocking();

        /// Poll the receive completion queue blocking
        uint64_t pollRecvCompletionQueueBlocking();

        /// Wait for a work request completion
        std::pair<bool, uint64_t> waitForCompletion();

        uint64_t waitForCompletionSend();

        uint64_t waitForCompletionReceive();
    };
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
