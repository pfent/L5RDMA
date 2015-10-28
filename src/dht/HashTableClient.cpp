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
#include "util/Utility.hpp"
#include "dht/HashTableNetworkLayout.hpp"
#include "dht/HashTableServer.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <iostream>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace dht {
//---------------------------------------------------------------------------
HashTableClient::HashTableClient(rdma::Network &network, HashTableNetworkLayout &remoteTables, HashTableServer &localServer, uint64_t entryCountPerHost)
        : network(network)
          , remoteTables(remoteTables)
          , localServer(localServer)
          , hostCount(remoteTables.remoteHashTables.size())
          , entryCountPerHost(entryCountPerHost)
          , totalEntryCount(hostCount * entryCountPerHost)
          , maskForPositionSelection(entryCountPerHost - 1)
          , maskForHostSelection((totalEntryCount - 1) xor maskForPositionSelection) // MATH !!!
          , shiftForHostSelection(util::getNumberOfSetBits(maskForPositionSelection))
{
   assert(util::isPowerOfTwo(hostCount));
   assert(util::isPowerOfTwo(entryCountPerHost));
   assert(util::isPowerOfTwo(totalEntryCount)); // (is implied, except overflow)
}
//---------------------------------------------------------------------------
HashTableClient::~HashTableClient()
{
}
//---------------------------------------------------------------------------
void HashTableClient::insert(const Entry &entry)
{
   cout << "insert: " << entry.key << endl;

   uint64_t targetHost = (entry.key & maskForHostSelection) >> shiftForHostSelection;
   uint64_t targetHtIndex = (entry.key & maskForPositionSelection);

   // Obtain memory for the bucket
   uint64_t newBucketOffset;
   {
      // Pin local memory
      vector <uint64_t> shared(1);
      rdma::MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

      // Create work request
      rdma::AtomicFetchAndAddWorkRequest workRequest;
      workRequest.setId(8028);
      workRequest.setLocalAddress(sharedMR);
      workRequest.setRemoteAddress(remoteTables.remoteHashTables[targetHost].nextFreeOffsetRmr);
      workRequest.setAddValue(1);
      workRequest.setCompletion(true);

      network.postWorkRequest(targetHost, workRequest);
      network.waitForCompletionSend();
      newBucketOffset = shared[0];
   }

   // Compare and swap the new value in
   uint64_t oldNextOffset;
   {
      // Pin local memory
      vector <uint64_t> oldValue(1);
      oldValue[0] = 0;
      rdma::MemoryRegion oldValueMR(oldValue.data(), sizeof(uint64_t) * oldValue.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

      // Create work request
      rdma::AtomicCompareAndSwapWorkRequest workRequest;
      workRequest.setId(8029);
      workRequest.setLocalAddress(oldValueMR);
      rdma::RemoteMemoryRegion htRmr = remoteTables.remoteHashTables[targetHost].htRmr;
      workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{htRmr.address + targetHtIndex * sizeof(uint64_t), htRmr.key});
      workRequest.setCompletion(true);
      workRequest.setSwapValue(newBucketOffset);

      // Try swapping until it works
      do {
         workRequest.setCompareValue(oldValue[0]);
         network.postWorkRequest(targetHost, workRequest);
         network.waitForCompletionSend();
      } while (oldValue[0] != workRequest.getCompareValue());
      oldNextOffset = oldValue[0];
   }

   // Write into the bucket
   {
      // Pin local memory
      vector <Bucket> bucket(1, {entry, oldNextOffset});
      rdma::MemoryRegion bucketMR(bucket.data(), sizeof(Bucket) * bucket.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

      // Create work request
      rdma::WriteWorkRequest workRequest;
      workRequest.setId(8029);
      workRequest.setLocalAddress(bucketMR);
      rdma::RemoteMemoryRegion bucketsRmr = remoteTables.remoteHashTables[targetHost].bucketsRmr;
      workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{bucketsRmr.address + newBucketOffset * sizeof(Bucket), bucketsRmr.key});
      workRequest.setCompletion(true);

      network.postWorkRequest(targetHost, workRequest);
      network.waitForCompletionSend();
   }
}
//---------------------------------------------------------------------------
uint32_t HashTableClient::count(uint64_t) const
{
   throw;
}
//---------------------------------------------------------------------------
void HashTableClient::dump() const
{
   cout << "> HashTableClient" << endl;
   cout << ">   hostCount= " << hostCount << endl;
   cout << ">   entryCountPerHost= " << entryCountPerHost << endl;
   cout << ">   maskForPositionSelection= " << maskForPositionSelection << endl;
   cout << ">   maskForHostSelection= " << maskForHostSelection << endl;
   cout << ">   shiftForHostSelection= " << shiftForHostSelection << endl;
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
