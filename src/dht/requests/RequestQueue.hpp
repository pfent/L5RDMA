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
#include <zmq.hpp>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
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
enum class RequestStatus : public uint8_t {
   FINISHED_AND_RECYCLE, SEND_AGAIN
};
//---------------------------------------------------------------------------
struct Request {
   virtual RequestStatus onDone() = 0;
   virtual rdma::WorkRequest &getReqeust() const = 0;
   virtual ~RequestStatus() { }
};
//---------------------------------------------------------------------------
struct InsertRequest : public Request {
   uint64_t targetHost;
   uint64_t targetHtIndex;

   void init(const Entry &entry)
   {
      targetHost = (entry.key & maskForHostSelection) >> shiftForHostSelection;
      targetHtIndex = (entry.key & maskForPositionSelection);

      // Write into the bucket
      {
         localServer.bucketMemory[bucketOffset].entry = entry;
         localServer.bucketMemory[bucketOffset].next = oldNextIdentifier;
      }

      // Obtain memory for the bucket
      uint64_t bucketOffset = localServer.nextFreeOffset++;
      BucketLocator bucketLocator(localHostId, bucketOffset);

      // Compare and swap the new value in
      BucketLocator oldNextIdentifier;
      {
         // TODO OPT: Pin local memory
         vector <BucketLocator> oldBucketLocator(1);
         rdma::MemoryRegion oldBucketLocatorMR(oldBucketLocator.data(), sizeof(uint64_t) * oldBucketLocator.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

         // Create work request
         rdma::AtomicCompareAndSwapWorkRequest workRequest;
         workRequest.setId(8029);
         workRequest.setLocalAddress(oldBucketLocatorMR);
         rdma::RemoteMemoryRegion htRmr = remoteTables.remoteHashTables[targetHost].htRmr;
         workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{htRmr.address + targetHtIndex * sizeof(BucketLocator), htRmr.key});
         workRequest.setCompletion(true);
         workRequest.setSwapValue(bucketLocator.data);

         // Try swapping until it works
         do {
            workRequest.setCompareValue(oldBucketLocator[0].data);
            network.postWorkRequest(targetHost, workRequest);
            network.waitForCompletionSend();
         } while (oldBucketLocator[0].data != workRequest.getCompareValue());
         oldNextIdentifier = oldBucketLocator[0];
      }
   }
};
//---------------------------------------------------------------------------
class RequestQueue : public util::NotAssignable {
public:
   RequestQueue(int bundleSize, int bundleCount);
   ~RequestQueue();

   void submit(Request &request);

   void finishAllOpenRequests();

private:
   struct Bundle {
      std::vector<Request *> requests;
   };

   std::vector <Bundle> bundles;
   uint32_t currentBundle;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
