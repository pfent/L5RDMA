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
#include <memory>

//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
struct ibv_qp;
struct ibv_cq;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    class WorkRequest;
    struct Address;

    class Network;

    class CompletionQueuePair;

    class ReceiveQueue;

//---------------------------------------------------------------------------
    class QueuePair {
        QueuePair(QueuePair const &) = delete;

        QueuePair &operator=(QueuePair const &) = delete;

        ibv_qp *qp;

        Network &network;

        CompletionQueuePair &completionQueuePair;

    public:
        QueuePair(Network &network); // Uses shared completion and receive Queue
        QueuePair(Network &network, ReceiveQueue &receiveQueue); // Uses shared completion Queue
        QueuePair(Network &network, CompletionQueuePair &completionQueuePair); // Uses shared receive Queue
        QueuePair(Network &network, CompletionQueuePair &completionQueuePair, ReceiveQueue &receiveQueue);

        ~QueuePair();

        uint32_t getQPN();

        void connect(const Address &address, unsigned retryCount = 0);

        void postWorkRequest(const WorkRequest &workRequest);

        uint32_t getMaxInlineSize();

        /// Print detailed information about this queue pair
        void printQueuePairDetails();

        CompletionQueuePair &getCompletionQueuePair() { return completionQueuePair; }
    };
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
