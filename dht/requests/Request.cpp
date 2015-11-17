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
#include "dht/requests/Request.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <iostream>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace dht {
//---------------------------------------------------------------------------
InsertRequest::InsertRequest(rdma::Network &network, util::InlineList<InsertRequest> &freeList)
        : oldBucketLocatorMR(&oldBucketLocator, sizeof(oldBucketLocator), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All)
          , freeList(freeList)
{
}
//---------------------------------------------------------------------------
InsertRequest::~InsertRequest()
{
}
//---------------------------------------------------------------------------
void InsertRequest::init(Bucket *bucket, const rdma::RemoteMemoryRegion &remoteTarget, const BucketLocator &localBucketLocation)
{
   this->bucket = bucket;

   // Init work request
   workRequest.setId((uintptr_t) this);
   workRequest.setLocalAddress(oldBucketLocatorMR);
   workRequest.setCompareValue(0);
   workRequest.setRemoteAddress(remoteTarget);
   workRequest.setSwapValue(localBucketLocation.data);
}
//---------------------------------------------------------------------------
RequestStatus InsertRequest::onCompleted()
{
   if (oldBucketLocator.data != workRequest.getCompareValue()) {
      workRequest.setCompareValue(oldBucketLocator.data);
      return RequestStatus::SEND_AGAIN;
   }

   bucket->next.data = oldBucketLocator.data;
   freeList.push(this);
   return RequestStatus::FINISHED;
}
//---------------------------------------------------------------------------
DummyRequest::DummyRequest(rdma::Network &network, rdma::RemoteMemoryRegion &remoteMemoryRegion)
        : memoryMR(&memory, sizeof(memory), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All)
{
   workRequest.setCompletion(true);
   workRequest.setLocalAddress(memoryMR);
   workRequest.setRemoteAddress(remoteMemoryRegion);
}
//---------------------------------------------------------------------------
DummyRequest::~DummyRequest()
{
}
//---------------------------------------------------------------------------
RequestStatus DummyRequest::onCompleted()
{
   return RequestStatus::FINISHED;
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
