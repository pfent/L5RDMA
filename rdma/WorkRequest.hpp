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
#include <vector>
#include "MemoryRegion.hpp"

//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace rdma {
    class QueuePair;

//---------------------------------------------------------------------------
    struct RemoteMemoryRegion;

//---------------------------------------------------------------------------
    class WorkRequest {
        friend class Network;

        friend class QueuePair;

    protected:
        std::unique_ptr<ibv_send_wr> wr;
        const WorkRequest *next;

        WorkRequest();

        WorkRequest(const WorkRequest &) = delete;

        WorkRequest(WorkRequest &&) = default;

        WorkRequest &operator=(WorkRequest &&) = default;

        const WorkRequest &operator=(const WorkRequest &) = delete;

        ~WorkRequest();

    public:

        /// Clear all set data and restore to original state after construction
        void reset();

        /// Assign an arbitrary id (not used, only for 'recognizing' it)
        void setId(uint64_t id);

        uint64_t getId() const;

        /// Should the work request produce a completion event
        void setCompletion(bool flag);

        bool getCompletion() const;

        /// Build a chain of work requests
        void setNextWorkRequest(const WorkRequest *workRequest);

        const WorkRequest *getNextWorkRequest();
    };

//---------------------------------------------------------------------------
    class RDMAWorkRequest : public WorkRequest { // Read & Write
    public:
        RDMAWorkRequest();

        /// Remote memory address
        /// For READ: location to be read from
        /// For WRITE: location to be written to
        void setRemoteAddress(const RemoteMemoryRegion &remoteAddress);

        /// Local memory address
        /// For READ: location to be written to
        /// For WRITE: location to be read from
        void setLocalAddress(const MemoryRegion &localAddress);

        void setLocalAddress(const MemoryRegion::Slice &localAddress);

        /// Set a variable number of MemoryRegions, from with data is sent / written to in a single request.
        /// The total message size then is the sum of all MRs
        void setLocalAddress(const std::vector<MemoryRegion::Slice> localAddresses);
    };

//---------------------------------------------------------------------------
    class ReadWorkRequest : public RDMAWorkRequest {
    public:
        ReadWorkRequest();

        static uint64_t getId() {
            return 42;
        }
    };

    class ReadWorkRequestBuilder {
        ReadWorkRequest wr;
    public:
        ReadWorkRequestBuilder(const MemoryRegion &localAddress, const RemoteMemoryRegion &remoteAddress,
                               bool completion);

        ReadWorkRequestBuilder(const MemoryRegion::Slice &localAddress, const RemoteMemoryRegion &remoteAddress,
                               bool completion);

        ReadWorkRequestBuilder &setNextWorkRequest(const WorkRequest *workRequest);

        void send(QueuePair &qp);

        ReadWorkRequest build();
    };

//---------------------------------------------------------------------------
    class WriteWorkRequest : public RDMAWorkRequest {
    public:
        WriteWorkRequest();

        /// Sets the IBV_SEND_INLINE flag.
        /// Only set this flag, if you know that the total message size is below the maximum inline message size. This
        /// depends on the QueuePair, specifically "qp_init_attr->cap->max_inline_data"
        /// This flag can only be set on Write / Send requests
        void setSendInline(bool flag);
    };

    class WriteWorkRequestBuilder {
        WriteWorkRequest wr;
        size_t size;
    public:
        WriteWorkRequestBuilder(const MemoryRegion &localAddress, const RemoteMemoryRegion &remoteAddress,
                                bool completion);

        WriteWorkRequestBuilder(const MemoryRegion::Slice &localAddress, const RemoteMemoryRegion &remoteAddress,
                                bool completion);

        WriteWorkRequestBuilder &setNextWorkRequest(const WorkRequest *workRequest);

        WriteWorkRequestBuilder &send(QueuePair &qp);

        WriteWorkRequestBuilder &setInline(bool flag);

        WriteWorkRequest build();
    };

//---------------------------------------------------------------------------
    class AtomicWorkRequest : public WorkRequest { // Fetch_Add & Compare_Swap
    public:
        AtomicWorkRequest();

        /// Remote memory address (location of the add value)
        void setRemoteAddress(const RemoteMemoryRegion &remoteAddress);

        /// Local memory address (location to write what was at the remote )
        void setLocalAddress(const MemoryRegion &localAddress);

        void setLocalAddress(const MemoryRegion::Slice &localAddress);
    };

//---------------------------------------------------------------------------
    class AtomicFetchAndAddWorkRequest : public AtomicWorkRequest {
    public:
        AtomicFetchAndAddWorkRequest();

        /// The number to be added to the remote address
        void setAddValue(uint64_t value);

        uint64_t getAddValue() const;
    };

    class AtomicFetchAndAddWorkRequestBuilder {
        AtomicFetchAndAddWorkRequest wr;
    public:
        AtomicFetchAndAddWorkRequestBuilder(const MemoryRegion &localAddress, const RemoteMemoryRegion &remoteAddress,
                                            uint64_t addValue,
                                            bool completion);

        AtomicFetchAndAddWorkRequestBuilder(const MemoryRegion::Slice &localAddress,
                                            const RemoteMemoryRegion &remoteAddress, uint64_t addValue,
                                            bool completion);

        AtomicFetchAndAddWorkRequestBuilder &setNextWorkRequest(const WorkRequest *workRequest);

        void send(QueuePair &qp);

        AtomicFetchAndAddWorkRequest build();
    };

//---------------------------------------------------------------------------
    class AtomicCompareAndSwapWorkRequest : public AtomicWorkRequest {
    public:
        AtomicCompareAndSwapWorkRequest();

        /// The number to be compared against
        void setCompareValue(uint64_t value);

        uint64_t getCompareValue() const;

        /// The number to be swapped in
        void setSwapValue(uint64_t value);

        uint64_t getSwapValue() const;
    };
//---------------------------------------------------------------------------
    static_assert(sizeof(rdma::WorkRequest) == sizeof(rdma::AtomicCompareAndSwapWorkRequest), "");
    static_assert(sizeof(rdma::WorkRequest) == sizeof(rdma::ReadWorkRequest), "");

//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
