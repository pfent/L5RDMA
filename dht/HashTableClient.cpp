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
#include "HashTableClient.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/Network.hpp"
#include "rdma/WorkRequest.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "util/Utility.hpp"
#include "dht/HashTableNetworkLayout.hpp"
#include "dht/HashTableServer.hpp"
#include "dht/requests/RequestQueue.hpp"
#include "util/FreeListAllocator.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <iostream>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace dht {
//---------------------------------------------------------------------------
HashTableClient::HashTableClient(rdma::Network &network, HashTableNetworkLayout &remoteTables, HashTableServer &localServer, uint64_t localHostId, uint64_t entryCountPerHost)
        : network(network)
          , remoteTables(remoteTables)
          , localServer(localServer)
          , localHostId(localHostId)
          , hostCount(remoteTables.remoteHashTables.size())
          , entryCountPerHost(entryCountPerHost)
          , totalEntryCount(hostCount * entryCountPerHost)
          , maskForPositionSelection(entryCountPerHost - 1)
          , maskForHostSelection((totalEntryCount - 1) xor maskForPositionSelection) // MATH !!!
          , shiftForHostSelection(util::getNumberOfSetBits(maskForPositionSelection))
{
   assert(util::isPowerOfTwo(hostCount));
   assert(util::isPowerOfTwo(entryCountPerHost));
   assert(util::isPowerOfTwo(totalEntryCount)); // (is implied, except overflow ?)
}
//---------------------------------------------------------------------------
HashTableClient::~HashTableClient()
{
}
//---------------------------------------------------------------------------
void HashTableClient::insert(const Entry &entry)
{
   // Determine target insert host and index
   uint64_t targetHost = (entry.key & maskForHostSelection) >> shiftForHostSelection;
   uint64_t targetHtIndex = (entry.key & maskForPositionSelection);
   rdma::RemoteMemoryRegion remoteHashTable = remoteTables.remoteHashTables[targetHost].htRmr;
   rdma::RemoteMemoryRegion remoteBucketLocator = {remoteHashTable.address + targetHtIndex * sizeof(BucketLocator), remoteHashTable.key};

   // Allocate a bucket
   uint64_t bucketOffset = localServer.nextFreeOffset++;
   assert(bucketOffset<localServer.bucketMemory.size());
   BucketLocator newBucketLocator(localHostId, bucketOffset);
   Bucket *newBucket = &localServer.bucketMemory[bucketOffset];
   newBucket->entry = entry; // next will be set once it was retrieved from the remote host

   // Fire off the request
   dht::InsertRequest *insertRequest = remoteTables.requestQueues[targetHost]->getInsertRequestPool().allocate();
   insertRequest->init(newBucket, remoteBucketLocator, newBucketLocator);
   remoteTables.requestQueues[targetHost]->submit(insertRequest);
}
//---------------------------------------------------------------------------
uint32_t HashTableClient::count(uint64_t key) const
{
   uint64_t targetHost = (key & maskForHostSelection) >> shiftForHostSelection;
   uint64_t targetHtIndex = (key & maskForPositionSelection);
   uint32_t result = 0;

   // Read from ht
   // TODO OPT: read directly if local
   BucketLocator next;
   {
      // TODO OPT: Pin local memory
      vector<BucketLocator> bucketLocator(1);
      rdma::MemoryRegion bucketLocatorMR(bucketLocator.data(), sizeof(bucketLocator[0]) * bucketLocator.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

      auto &remote = remoteTables.remoteHashTables[targetHost];
      auto &remoteLocation = remoteTables.locations[targetHost];

      rdma::ReadWorkRequest workRequest;
      workRequest.setId(8029);
      workRequest.setLocalAddress(bucketLocatorMR);
      workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{remote.htRmr.address + targetHtIndex * sizeof(BucketLocator), remote.htRmr.key});
      workRequest.setCompletion(true);
      remoteLocation.queuePair->postWorkRequest(workRequest);
      remoteLocation.queuePair->getCompletionQueuePair().waitForCompletionSend();
      next = bucketLocator[0];
   }

   // Follow ptr chain
   // TODO OPT: Pin local memory
   vector<Bucket> bucket(1);
   rdma::MemoryRegion bucketMR(bucket.data(), sizeof(Bucket) * bucket.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);
   {
      while (next.data) {
         // Read from remote host
         Bucket b;
         {
            auto &remote = remoteTables.remoteHashTables[next.getHost()];
            auto &remoteLocation = remoteTables.locations[next.getHost()];

            rdma::ReadWorkRequest workRequest;
            workRequest.setId(8029);
            workRequest.setLocalAddress(bucketMR);
            workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{remote.bucketsRmr.address + next.getOffset() * sizeof(Bucket), remote.bucketsRmr.key});
            workRequest.setCompletion(true);
            remoteLocation.queuePair->postWorkRequest(workRequest);
            remoteLocation.queuePair->getCompletionQueuePair().waitForCompletionSend();
            b = bucket[0];
         }

         if (b.entry.key == key)
            result++;
         next = b.next;
      }
   }

   return result;
}
//---------------------------------------------------------------------------
void HashTableClient::dump() const
{
   cout << "> HashTableClient" << endl;
   cout << ">   localHostId= " << localHostId << endl;
   cout << ">   hostCount= " << hostCount << endl;
   cout << ">   entryCountPerHost= " << entryCountPerHost << endl;
   cout << ">   maskForPositionSelection= " << maskForPositionSelection << endl;
   cout << ">   maskForHostSelection= " << maskForHostSelection << endl;
   cout << ">   shiftForHostSelection= " << shiftForHostSelection << endl;
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
