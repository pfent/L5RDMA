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
#include "HashTableServer.hpp"
#include "HashTableNetworkLayout.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/Network.hpp"
#include "rdma/WorkRequest.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "util/Utility.hpp"
//---------------------------------------------------------------------------
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cassert>
#include <infiniband/verbs.h>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace dht {
//---------------------------------------------------------------------------
HashTableServer::HashTableServer(rdma::Network &network, uint32_t htSize, uint32_t maxBucketCount)
        : htMemory(htSize, BucketLocator(0, 0))
          , bucketMemory(maxBucketCount)
          , nextFreeOffset(1) // First bucket is the "invalid bucket"
          , network(network)
          , running(true)
{
   // Pin memory regions
   htMr = make_unique<rdma::MemoryRegion>(htMemory.data(), sizeof(htMemory[0]) * htSize, network.getProtectionDomain(), rdma::MemoryRegion::Permission::All); // All - because who cares, it's research :)
   bucketsMr = make_unique<rdma::MemoryRegion>(bucketMemory.data(), sizeof(bucketMemory[0]) * maxBucketCount, network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);
   nextFreeOffsetMr = make_unique<rdma::MemoryRegion>(&nextFreeOffset, sizeof(uint64_t), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

   // Check alignment
   assert((((uint64_t) htMr->address) & 0x7) == 0);
   assert((((uint64_t) bucketsMr->address) & 0x7) == 0);
   assert((((uint64_t) &nextFreeOffset) & 0x7) == 0);
}
//---------------------------------------------------------------------------
HashTableServer::~HashTableServer()
{
}
//---------------------------------------------------------------------------
void HashTableServer::startAddressServiceAsync(zmq::context_t &context, string hostname, int port)
{
   this->hostname = hostname;

   // Create socket
   socket = make_unique<zmq::socket_t>(context, ZMQ_REP);
   socket->bind((string("tcp://*:") + util::to_string(port)).c_str());

   // Prepare a message (outside an actual message because zmq destroys messages when sending them)
   vector<rdma::RemoteMemoryRegion> messageMemory(3);
   rdma::RemoteMemoryRegion htRmr{reinterpret_cast<uintptr_t>(htMr->address), htMr->key->rkey};
   rdma::RemoteMemoryRegion bucketsRmr{reinterpret_cast<uintptr_t>(bucketsMr->address), bucketsMr->key->rkey};
   rdma::RemoteMemoryRegion nextFreeOffsetRmr{reinterpret_cast<uintptr_t>(nextFreeOffsetMr->address), nextFreeOffsetMr->key->rkey};
   memcpy(messageMemory.data() + 0, &htRmr, sizeof(rdma::RemoteMemoryRegion));
   memcpy(messageMemory.data() + 1, &bucketsRmr, sizeof(rdma::RemoteMemoryRegion));
   memcpy(messageMemory.data() + 2, &nextFreeOffsetRmr, sizeof(rdma::RemoteMemoryRegion));

   thread = make_unique<::thread>([=, &context]() {
      // Send the addresses to each one who asks
      while (running) {
         zmq::pollitem_t items[] = {(void*)*socket, 0, ZMQ_POLLIN, 0};
         int rc = zmq_poll(items, 1, 1000); // 1s

         if (rc<0) {
            cout << errno << endl;
            cout << zmq_errno() << endl;
            cout << zmq_strerror(zmq_errno()) << endl;
            throw;
         }

         if (items[0].revents != 0) {
            zmq::message_t request;
            socket->recv(&request);
            assert(request.size() == 0); // :p .. we don't need info from the peers

            zmq::message_t reply(sizeof(rdma::RemoteMemoryRegion) * 3);
            memcpy(reply.data(), messageMemory.data(), sizeof(rdma::RemoteMemoryRegion) * 3);
            socket->send(reply);
         }
      }
   });
}
//---------------------------------------------------------------------------
void HashTableServer::stopAddressService()
{
   running = false;
   thread->join();
}
//---------------------------------------------------------------------------
void HashTableServer::dumpMemoryRegions()
{
   cout << "> Created HT Server" << endl;
   cout << ">   htMr= {" << *htMr << "}" << endl;
   cout << ">   bucketsMr= {" << *bucketsMr << "}" << endl;
   cout << ">   nextFreeOffsetMr= " << *nextFreeOffsetMr << "}" << endl;
   if (socket == nullptr) {
      cout << ">   address distribution service NOT started." << endl;
   } else {
      cout << ">   listenting on: {hostname=" << hostname << " port=" << port << "}" << endl;
   }
   cout << "> [End]" << endl;
}
//---------------------------------------------------------------------------
void HashTableServer::dumpHashTableContent(HashTableNetworkLayout &hashTableNetworkLayout)
{
   // Pin local memory
   vector<Bucket> bucket(1);
   rdma::MemoryRegion bucketMR(bucket.data(), sizeof(Bucket) * bucket.size(), network.getProtectionDomain(), rdma::MemoryRegion::Permission::All);

   for (uint i = 0; i<htMemory.size(); ++i) {
      cout << "[" << i << "]";
      BucketLocator next = htMemory[i];
      while (next.data) {
         // Read from remote host
         Bucket b;
         {
            auto &remote = hashTableNetworkLayout.remoteHashTables[next.getHost()];
            auto &remoteLocation = hashTableNetworkLayout.locations[next.getHost()];

            rdma::ReadWorkRequest workRequest;
            workRequest.setId(8029);
            workRequest.setLocalAddress(bucketMR);
            workRequest.setRemoteAddress(rdma::RemoteMemoryRegion{remote.bucketsRmr.address + next.getOffset() * sizeof(Bucket), remote.bucketsRmr.key});
            workRequest.setCompletion(true);
            remoteLocation.queuePair->postWorkRequest(workRequest);
            remoteLocation.queuePair->getCompletionQueuePair().waitForCompletionSend();
            b = bucket[0];
         }

         cout << " -> " << next.getHost() << ":" << next.getOffset() << flush;
         cout << " [key=" << b.entry.key << "]" << flush;
         next = b.next;
      }
      cout << " -> ∅" << endl;
   }
   cout << "next = " << nextFreeOffset << endl;
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
