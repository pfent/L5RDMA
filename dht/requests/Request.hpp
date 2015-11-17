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
#include "util/InlineList.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
namespace rdma {
class Network;
}
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
enum struct RequestStatus : uint8_t {
   FINISHED, SEND_AGAIN
};
//---------------------------------------------------------------------------
// A Request can be posted to RequestQueue
// The Request::workRequestPtr will be sent and the Request::onCompleted callback is invoked once its done
// Request::onCompleted can either return RequestStatus::FINISHED or RequestStatus::SEND_AGAIN
// The latter one will cause the queue to send the Request again, otherwise it is dropped
struct Request : public util::NotAssignable {
   virtual ~Request() { }

   virtual RequestStatus onCompleted() = 0;
   virtual rdma::WorkRequest *getRequest() = 0;
};
//---------------------------------------------------------------------------
// Used to inserts elements into a remote hash table
// To avoid allocations (and memory pins): The InsertRequest is inserted into the InsertRequest::list once it is completed (FINISHED).
// The InsertRequests in the InsertRequest::list, are reused by the HashTableClient
struct InsertRequest : public Request, public util::InlineListElement<InsertRequest> {
   rdma::AtomicCompareAndSwapWorkRequest workRequest;

   BucketLocator oldBucketLocator;
   rdma::MemoryRegion oldBucketLocatorMR;

   Bucket *bucket;

   util::InlineList<InsertRequest> &freeList; // Object inserts itself into this list once its finished

   InsertRequest(rdma::Network &network, util::InlineList<InsertRequest> &freeListAllocator);
   virtual ~InsertRequest();

   void init(Bucket *bucket, const rdma::RemoteMemoryRegion &remoteTarget, const BucketLocator &localBucketLocation);

   virtual RequestStatus onCompleted();
   virtual rdma::WorkRequest *getRequest() { return &workRequest; };
};
//---------------------------------------------------------------------------
// Used to send a noop with a completion request
struct DummyRequest : public Request {
   rdma::ReadWorkRequest workRequest;

   uint64_t memory;
   rdma::MemoryRegion memoryMR;

   DummyRequest(rdma::Network &network, rdma::RemoteMemoryRegion &remoteMemoryRegion);
   virtual ~DummyRequest();

   virtual RequestStatus onCompleted();
   virtual rdma::WorkRequest *getRequest() { return &workRequest; };
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
