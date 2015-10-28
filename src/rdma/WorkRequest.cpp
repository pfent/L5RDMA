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
#include "MemoryRegion.hpp"
#include "Network.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <cstring>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
WorkRequest::WorkRequest()
{
   wr = unique_ptr<ibv_send_wr>(new ibv_send_wr());
   wr->sg_list = new ibv_sge();
   reset();
}
//---------------------------------------------------------------------------
WorkRequest::~WorkRequest()
{
   delete wr->sg_list;
}
//---------------------------------------------------------------------------
void WorkRequest::reset()
{
   auto tmp = wr->sg_list;
   memset(wr.get(), 0, sizeof(ibv_send_wr));
   wr->sg_list = tmp;
   wr->num_sge = 1;
   memset(wr->sg_list, 0, sizeof(ibv_sge));
}
//---------------------------------------------------------------------------
void WorkRequest::setId(uint64_t id)
{
   wr->wr_id = id;
}
//---------------------------------------------------------------------------
uint64_t WorkRequest::getId() const
{
   return wr->wr_id;
}
//---------------------------------------------------------------------------
void WorkRequest::setCompletion(bool flag)
{
   if (flag)
      wr->send_flags = wr->send_flags | IBV_SEND_SIGNALED;
   else
      wr->send_flags = wr->send_flags & !IBV_SEND_SIGNALED;
}
//---------------------------------------------------------------------------
bool WorkRequest::getCompletion() const
{
   return wr->send_flags & IBV_SEND_SIGNALED;
}
//---------------------------------------------------------------------------
void WorkRequest::setNextWorkRequest(const WorkRequest *workRequest)
{
   next = workRequest;
   if (next == nullptr)
      wr->next = nullptr;
   else
      wr->next = workRequest->wr.get();
}
//---------------------------------------------------------------------------
const WorkRequest *WorkRequest::getNextWorkRequest()
{
   return next;
}
//---------------------------------------------------------------------------
RDMAWorkRequest::RDMAWorkRequest()
{
}
//---------------------------------------------------------------------------
void RDMAWorkRequest::setLocalAddress(const MemoryRegion &localAddress)
{
   wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
   wr->sg_list->length = localAddress.size;
   wr->sg_list->lkey = localAddress.key->lkey;
}
//---------------------------------------------------------------------------
void RDMAWorkRequest::setRemoteAddress(const RemoteMemoryRegion &remoteAddress)
{
   wr->wr.rdma.remote_addr = remoteAddress.address;
   wr->wr.rdma.rkey = remoteAddress.key;
}
//---------------------------------------------------------------------------
WriteWorkRequest::WriteWorkRequest()
{
   wr->opcode = IBV_WR_RDMA_WRITE;
}
//---------------------------------------------------------------------------
ReadWorkRequest::ReadWorkRequest()
{
   wr->opcode = IBV_WR_RDMA_READ;
}
//---------------------------------------------------------------------------
AtomicWorkRequest::AtomicWorkRequest()
{
}
//---------------------------------------------------------------------------
void AtomicWorkRequest::setRemoteAddress(const RemoteMemoryRegion &remoteAddress)
{
   wr->wr.atomic.remote_addr = remoteAddress.address;
   wr->wr.atomic.rkey = remoteAddress.key;
}
//---------------------------------------------------------------------------
void AtomicWorkRequest::setLocalAddress(const MemoryRegion &localAddress)
{
   wr->sg_list->addr = reinterpret_cast<uintptr_t>(localAddress.address);
   wr->sg_list->length = localAddress.size;
   wr->sg_list->lkey = localAddress.key->lkey;
}
//---------------------------------------------------------------------------
AtomicFetchAndAddWorkRequest::AtomicFetchAndAddWorkRequest()
{
   wr->opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
}
//---------------------------------------------------------------------------
void AtomicFetchAndAddWorkRequest::setAddValue(uint64_t value)
{
   wr->wr.atomic.compare_add = value;
}
//---------------------------------------------------------------------------
uint64_t AtomicFetchAndAddWorkRequest::getAddValue() const
{
   return wr->wr.atomic.compare_add;
}
//---------------------------------------------------------------------------
AtomicCompareAndSwapWorkRequest::AtomicCompareAndSwapWorkRequest()
{
   wr->opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
}
//---------------------------------------------------------------------------
void AtomicCompareAndSwapWorkRequest::setCompareValue(uint64_t value)
{
   wr->wr.atomic.compare_add = value;
}
//---------------------------------------------------------------------------
uint64_t AtomicCompareAndSwapWorkRequest::getCompareValue() const
{
   return wr->wr.atomic.compare_add;
}
//---------------------------------------------------------------------------
void AtomicCompareAndSwapWorkRequest::setSwapValue(uint64_t value)
{
   wr->wr.atomic.swap = value;
}
//---------------------------------------------------------------------------
uint64_t AtomicCompareAndSwapWorkRequest::getSwapValue() const
{
   return wr->wr.atomic.swap;
}
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
