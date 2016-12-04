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
struct ibv_srq;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    class Network;

//---------------------------------------------------------------------------
    class ReceiveQueue {
        friend class QueuePair;

        friend class CompletionQueuePair;

        ReceiveQueue(ReceiveQueue const &) = delete;

        ReceiveQueue &operator=(ReceiveQueue const &) = delete;

        /// The receive queue
        ibv_srq *queue;
    public:
        /// Ctor
        ReceiveQueue(Network &network);

        ~ReceiveQueue();
    };
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
