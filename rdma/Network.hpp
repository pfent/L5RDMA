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
#include <mutex>
#include <stdexcept>
#include <vector>
#include <memory>

//---------------------------------------------------------------------------
struct ibv_comp_channel;
struct ibv_context;
struct ibv_cq;
struct ibv_device;
struct ibv_mr;
struct ibv_pd;
struct ibv_qp;
struct ibv_srq;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    class WorkRequest;

    class MemoryRegion;

    class CompletionQueuePair;

    class ReceiveQueue;

//---------------------------------------------------------------------------
/// A network exception
    class NetworkException : public std::runtime_error {
    public:
        NetworkException(const std::string &reason)
                : std::runtime_error(reason) {}
    };

//---------------------------------------------------------------------------
    struct RemoteMemoryRegion {
        uintptr_t address;
        uint32_t key;

        RemoteMemoryRegion() = default;

        RemoteMemoryRegion(uintptr_t address, uint32_t key) : address(address), key(key) {}

        RemoteMemoryRegion slice(size_t offset);
    };

    std::ostream &operator<<(std::ostream &os, const RemoteMemoryRegion &remoteMemoryRegion);

//---------------------------------------------------------------------------
/// The LID and QPN uniquely address a queue pair
    struct Address {
        uint32_t qpn;
        uint16_t lid;
    };

    std::ostream &operator<<(std::ostream &os, const Address &address);

//---------------------------------------------------------------------------
/// Abstracts a global rdma context
    class Network {
        friend class CompletionQueuePair;

        friend class ReceiveQueue;

        friend class QueuePair;

        /// The minimal number of entries for the completion queue
        static const int CQ_SIZE = 100;

        /// The port of the Infiniband device
        uint8_t ibport;

        /// The Infiniband devices
        ibv_device **devices;
        /// The verbs context
        ibv_context *context;
        /// The global protection domain
        ibv_pd *protectionDomain;

        /// Shared Queues
        std::unique_ptr<CompletionQueuePair> sharedCompletionQueuePair;
        std::unique_ptr<ReceiveQueue> sharedReceiveQueue;

    public:
        /// Constructor
        Network();

        /// Destructor
        ~Network();

        /// Get the LID
        uint16_t getLID();

        /// Get the protection domain
        ibv_pd *getProtectionDomain() { return protectionDomain; }

        /// Print the capabilities of the RDMA host channel adapter
        void printCapabilities();
    };
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
