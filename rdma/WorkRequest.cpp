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
// ---------------------------------------------------------------------------
#include "WorkRequest.hpp"
#include "Network.hpp"
#include "QueuePair.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <cstring>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    WorkRequest::WorkRequest() {
        wr = unique_ptr<ibv_send_wr>(new ibv_send_wr());
        wr->sg_list = new ibv_sge();
        reset();
    }

//---------------------------------------------------------------------------
    WorkRequest::~WorkRequest() {
        delete wr->sg_list;
    }

//---------------------------------------------------------------------------
    void WorkRequest::reset() {
        auto tmp = wr->sg_list;
        memset(wr.get(), 0, sizeof(ibv_send_wr));
        wr->sg_list = tmp;
        wr->num_sge = 1;
        memset(wr->sg_list, 0, sizeof(ibv_sge));
    }

//---------------------------------------------------------------------------
    void WorkRequest::setId(uint64_t id) {
        wr->wr_id = id;
    }

//---------------------------------------------------------------------------
    uint64_t WorkRequest::getId() const {
        return wr->wr_id;
    }

//---------------------------------------------------------------------------
    void WorkRequest::setCompletion(bool flag) {
        if (flag)
            wr->send_flags = wr->send_flags | IBV_SEND_SIGNALED;
        else
            wr->send_flags = wr->send_flags & ~IBV_SEND_SIGNALED;
    }

//---------------------------------------------------------------------------
    bool WorkRequest::getCompletion() const {
        return wr->send_flags & IBV_SEND_SIGNALED;
    }

//---------------------------------------------------------------------------
    void WorkRequest::setNextWorkRequest(const WorkRequest *workRequest) {
        next = workRequest;
        if (next == nullptr)
            wr->next = nullptr;
        else
            wr->next = workRequest->wr.get();
    }

//---------------------------------------------------------------------------
    const WorkRequest *WorkRequest::getNextWorkRequest() {
        return next;
    }

//---------------------------------------------------------------------------
    RDMAWorkRequest::RDMAWorkRequest() {
    }

//---------------------------------------------------------------------------
    void RDMAWorkRequest::setLocalAddress(const MemoryRegion &localAddress) {
        wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
        wr->sg_list->length = localAddress.size;
        wr->sg_list->lkey = localAddress.key->lkey;
    }

//---------------------------------------------------------------------------
    void RDMAWorkRequest::setLocalAddress(const MemoryRegion::Slice &localAddress) {
        wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
        wr->sg_list->length = localAddress.size;
        wr->sg_list->lkey = localAddress.lkey;
    }

//---------------------------------------------------------------------------
    void RDMAWorkRequest::setRemoteAddress(const RemoteMemoryRegion &remoteAddress) {
        wr->wr.rdma.remote_addr = remoteAddress.address;
        wr->wr.rdma.rkey = remoteAddress.key;
    }

    void RDMAWorkRequest::setLocalAddress(const std::vector<MemoryRegion::Slice> localAddresses) {
        delete wr->sg_list;
        wr->sg_list = new ibv_sge[localAddresses.size()]();
        wr->num_sge = localAddresses.size();
        for (size_t i = 0; i < localAddresses.size(); ++i) {
            wr->sg_list[i].addr = reinterpret_cast<uintptr_t>(localAddresses[i].address);
            wr->sg_list[i].length = localAddresses[i].size;
            wr->sg_list[i].lkey = localAddresses[i].lkey;
        }

        // TODO: manage a sensible way of keeping track of the new'ed array
    }

//---------------------------------------------------------------------------
    WriteWorkRequest::WriteWorkRequest() {
        wr->opcode = IBV_WR_RDMA_WRITE;
    }

    void WriteWorkRequest::setSendInline(bool flag) {
        if (flag) {
            wr->send_flags |= IBV_SEND_INLINE;
        } else {
            wr->send_flags &= ~IBV_SEND_INLINE;
        }
    }

//---------------------------------------------------------------------------
    ReadWorkRequest::ReadWorkRequest() {
        wr->opcode = IBV_WR_RDMA_READ;
    }

//---------------------------------------------------------------------------
    AtomicWorkRequest::AtomicWorkRequest() {
    }

//---------------------------------------------------------------------------
    void AtomicWorkRequest::setRemoteAddress(const RemoteMemoryRegion &remoteAddress) {
        wr->wr.atomic.remote_addr = remoteAddress.address;
        wr->wr.atomic.rkey = remoteAddress.key;
    }

//---------------------------------------------------------------------------
    void AtomicWorkRequest::setLocalAddress(const MemoryRegion &localAddress) {
        wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
        wr->sg_list->length = localAddress.size;
        wr->sg_list->lkey = localAddress.key->lkey;
    }

    void AtomicWorkRequest::setLocalAddress(const MemoryRegion::Slice &localAddress) {
        wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
        wr->sg_list->length = localAddress.size;
        wr->sg_list->lkey = localAddress.lkey;
    }

//---------------------------------------------------------------------------
    AtomicFetchAndAddWorkRequest::AtomicFetchAndAddWorkRequest() {
        wr->opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    }

//---------------------------------------------------------------------------
    void AtomicFetchAndAddWorkRequest::setAddValue(uint64_t value) {
        wr->wr.atomic.compare_add = value;
    }

//---------------------------------------------------------------------------
    uint64_t AtomicFetchAndAddWorkRequest::getAddValue() const {
        return wr->wr.atomic.compare_add;
    }

//---------------------------------------------------------------------------
    AtomicCompareAndSwapWorkRequest::AtomicCompareAndSwapWorkRequest() {
        wr->opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
    }

//---------------------------------------------------------------------------
    void AtomicCompareAndSwapWorkRequest::setCompareValue(uint64_t value) {
        wr->wr.atomic.compare_add = value;
    }

//---------------------------------------------------------------------------
    uint64_t AtomicCompareAndSwapWorkRequest::getCompareValue() const {
        return wr->wr.atomic.compare_add;
    }

//---------------------------------------------------------------------------
    void AtomicCompareAndSwapWorkRequest::setSwapValue(uint64_t value) {
        wr->wr.atomic.swap = value;
    }

//---------------------------------------------------------------------------
    uint64_t AtomicCompareAndSwapWorkRequest::getSwapValue() const {
        return wr->wr.atomic.swap;
    }

//---------------------------------------------------------------------------
    void ReadWorkRequestBuilder::send(QueuePair &qp) {
        qp.postWorkRequest(wr);
    }

    ReadWorkRequestBuilder::ReadWorkRequestBuilder(const MemoryRegion &localAddress,
                                                   const RemoteMemoryRegion &remoteAddress,
                                                   bool completion) : wr() {
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
        wr.setId(42);
    }

    ReadWorkRequestBuilder &ReadWorkRequestBuilder::setNextWorkRequest(const WorkRequest *workRequest) {
        wr.setNextWorkRequest(workRequest);
        return *this;
    }

    ReadWorkRequest ReadWorkRequestBuilder::build() {
        return move(wr);
    }

    ReadWorkRequestBuilder::ReadWorkRequestBuilder(const MemoryRegion::Slice &localAddress,
                                                   const RemoteMemoryRegion &remoteAddress,
                                                   bool completion) : wr() {
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
    }

    WriteWorkRequestBuilder::WriteWorkRequestBuilder(const MemoryRegion &localAddress,
                                                     const RemoteMemoryRegion &remoteAddress,
                                                     bool completion) : wr() {
        size = localAddress.size;
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
    }

    WriteWorkRequestBuilder &WriteWorkRequestBuilder::send(QueuePair &qp) {
        qp.postWorkRequest(wr);
        return *this;
    }

    WriteWorkRequestBuilder &WriteWorkRequestBuilder::setNextWorkRequest(const WorkRequest *workRequest) {
        wr.setNextWorkRequest(workRequest);
        return *this;
    }

    WriteWorkRequest WriteWorkRequestBuilder::build() {
        return move(wr);
    }

    WriteWorkRequestBuilder::WriteWorkRequestBuilder(const MemoryRegion::Slice &localAddress,
                                                     const RemoteMemoryRegion &remoteAddress,
                                                     bool completion) : wr() {
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
    }

    WriteWorkRequestBuilder &WriteWorkRequestBuilder::setInline(bool sendInline) {
        wr.setSendInline(sendInline);
        return *this;
    }

    AtomicFetchAndAddWorkRequestBuilder::AtomicFetchAndAddWorkRequestBuilder(const MemoryRegion &localAddress,
                                                                             const RemoteMemoryRegion &remoteAddress,
                                                                             uint64_t addValue,
                                                                             bool completion) {
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
        wr.setAddValue(addValue);
    }

    void AtomicFetchAndAddWorkRequestBuilder::send(QueuePair &qp) {
        qp.postWorkRequest(wr);
    }

    AtomicFetchAndAddWorkRequestBuilder &
    AtomicFetchAndAddWorkRequestBuilder::setNextWorkRequest(const WorkRequest *workRequest) {
        wr.setNextWorkRequest(workRequest);
        return *this;
    }

    AtomicFetchAndAddWorkRequest AtomicFetchAndAddWorkRequestBuilder::build() {
        return move(wr);
    }

    AtomicFetchAndAddWorkRequestBuilder::AtomicFetchAndAddWorkRequestBuilder(const MemoryRegion::Slice &localAddress,
                                                                             const RemoteMemoryRegion &remoteAddress,
                                                                             uint64_t addValue,
                                                                             bool completion) {
        wr.setLocalAddress(localAddress);
        wr.setRemoteAddress(remoteAddress);
        wr.setCompletion(completion);
        wr.setAddValue(addValue);
    }
} // End of namespace rdma
//---------------------------------------------------------------------------
