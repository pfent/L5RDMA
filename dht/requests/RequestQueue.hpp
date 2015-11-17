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
#include <array>
#include <vector>
#include <iostream>
#include <zmq.hpp>
#include <util/FreeListAllocator.hpp>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct HashTableNetworkLayout;
struct HashTableServer;
//---------------------------------------------------------------------------
enum struct RequestStatus : uint8_t {
   FINISHED, SEND_AGAIN
};
//---------------------------------------------------------------------------
struct Request : public util::NotAssignable {
   virtual RequestStatus onCompleted() = 0;
   virtual rdma::WorkRequest &getRequest() = 0;
   virtual ~Request() { }
};
using namespace std;
//---------------------------------------------------------------------------
struct InsertRequest : public Request, public util::FreeListElement<InsertRequest> {
   rdma::AtomicCompareAndSwapWorkRequest workRequest;

   BucketLocator oldBucketLocator;
   rdma::MemoryRegion oldBucketLocatorMR;

   Bucket *bucket;

   util::FreeListAllocator<InsertRequest> &freeListAllocator;

   InsertRequest(rdma::Network &network, util::FreeListAllocator<InsertRequest> &freeListAllocator)
           : oldBucketLocatorMR(&oldBucketLocator, sizeof(oldBucketLocator), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All)
             , freeListAllocator(freeListAllocator)
   {
   }

   virtual ~InsertRequest() { }

   virtual RequestStatus onCompleted()
   {
      if (oldBucketLocator.data != workRequest.getCompareValue()) {
         workRequest.setCompareValue(oldBucketLocator.data);
         return RequestStatus::SEND_AGAIN;
      }

      bucket->next.data = oldBucketLocator.data;
      freeListAllocator.free(this);
      return RequestStatus::FINISHED;
   }

   void init(Bucket *bucket, const rdma::RemoteMemoryRegion &remoteTarget, const BucketLocator &localBucketLocation)
   {
      this->bucket = bucket;

      // Init work request
      workRequest.setId((uintptr_t) this);
      workRequest.setLocalAddress(oldBucketLocatorMR);
      workRequest.setCompareValue(0);
      workRequest.setRemoteAddress(remoteTarget);
      workRequest.setSwapValue(localBucketLocation.data);
   }

   virtual rdma::WorkRequest &getRequest() { return workRequest; }
};
//---------------------------------------------------------------------------
struct DummyRequest : public Request {
   rdma::ReadWorkRequest workRequest;

   uint64_t memory;
   rdma::MemoryRegion memoryMR;

   DummyRequest(rdma::Network &network, rdma::RemoteMemoryRegion &remoteMemoryRegion)
           : memoryMR(&memory, sizeof(memory), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All)
   {
      workRequest.setCompletion(true);
      workRequest.setLocalAddress(memoryMR);
      workRequest.setRemoteAddress(remoteMemoryRegion);
   }

   ~DummyRequest() { }

   virtual RequestStatus onCompleted()
   {
      return RequestStatus::FINISHED;
   }

   virtual rdma::WorkRequest &getRequest() { return workRequest; }
};
//---------------------------------------------------------------------------
class RequestQueue : public util::NotAssignable {
public:
   RequestQueue(rdma::Network &network, uint bundleSize, uint bundleCount, rdma::QueuePair &queuePair, DummyRequest &dummyRequest)
           : queuePair(queuePair)
             , bundles(bundleCount, Bundle{vector<Request *>(bundleSize)})
             , bundleSize(bundleSize)
             , bundleCount(bundleCount)
             , currentBundle(0)
             , nextWorkRequestInBundle(0)
             , bundleUpForCompletion(0)
             , dummyRequest(dummyRequest)
             , sentDummyCount(0)
             , sentRequestCount(0)
             , insertRequestPool(std::vector<InsertRequest *>())
   {
      for (uint i = 0; i<bundleSize * bundleCount + 1; ++i) {
         insertRequestPool.free(new InsertRequest(network, insertRequestPool)); // TODO Leak
      }
   }

   ~RequestQueue()
   {
      cout << "sentDummyCount = " << sentDummyCount << endl;
      cout << "sentRequestCount = " << sentRequestCount << endl;
   }

   void submit(Request *request)
   {
      // Wait
      while (nextWorkRequestInBundle>=bundleSize) {
         nextWorkRequestInBundle = 0;
         currentBundle = (currentBundle + 1) % bundleCount;

         // Wait until the bundle is completed
         if (currentBundle == bundleUpForCompletion) {
            // Wait for completion event
            queuePair.getCompletionQueuePair().waitForCompletionSend();
            bundleUpForCompletion = (bundleUpForCompletion + 1) % bundleCount;

            // Notify all request of this bundle that they are completed
            for (auto iter : bundles[currentBundle].requests)
               if (iter->onCompleted() == RequestStatus::SEND_AGAIN)
                  send(iter);
         }
      }

      // Send
      send(request);
   }

   void finishAllOpenRequests()
   {
      // Find all open work request
      std::vector<Request *> openRequests;
      while (nextWorkRequestInBundle != 0 || currentBundle != bundleUpForCompletion) {
         if (nextWorkRequestInBundle == 0) {
            currentBundle = (currentBundle - 1 + bundleCount) % bundleCount;
            nextWorkRequestInBundle = bundleSize;
         } else {
            nextWorkRequestInBundle--;
            openRequests.push_back(bundles[currentBundle].requests[nextWorkRequestInBundle]);
         }
      }

      // Wait for completions
      for (uint i = 0; i<openRequests.size() / bundleSize; ++i)
         queuePair.getCompletionQueuePair().waitForCompletionSend();

      // Make them finish
      while (!openRequests.empty()) {
         sentDummyCount++;
         queuePair.postWorkRequest(dummyRequest.getRequest());
         queuePair.getCompletionQueuePair().waitForCompletionSend();

         std::vector<Request *> stillOpenRequests;
         for (auto iter : openRequests) {
            if (iter->onCompleted() == RequestStatus::SEND_AGAIN) {
               iter->getRequest().setCompletion(false);
               sentRequestCount++;
               queuePair.postWorkRequest(iter->getRequest());
               stillOpenRequests.push_back(iter);
            }
         }
         swap(openRequests, stillOpenRequests);
      }
   }

   util::FreeListAllocator<InsertRequest> &getInsertRequestPool() { return insertRequestPool; }

private:
   void send(Request *request)
   {
      sentRequestCount++;
      bundles[currentBundle].requests[nextWorkRequestInBundle++] = request;
      request->getRequest().setCompletion(nextWorkRequestInBundle == bundleSize);
      queuePair.postWorkRequest(request->getRequest());
   }

   struct Bundle {
      std::vector<Request *> requests;
   };

   rdma::QueuePair &queuePair;
   std::vector<Bundle> bundles;
   const uint32_t bundleSize;
   const uint32_t bundleCount;
   uint32_t currentBundle;
   uint32_t nextWorkRequestInBundle;
   uint32_t bundleUpForCompletion;
   DummyRequest &dummyRequest;

   uint64_t sentDummyCount;
   uint64_t sentRequestCount;

   util::FreeListAllocator<InsertRequest> insertRequestPool;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
