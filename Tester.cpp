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
#include "rdma/Network.hpp"
#include "dht/Common.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
#include "util/ConnectionSetup.hpp"
#include "util/Utility.hpp"
#include "dht/HashTableClient.hpp"
#include "dht/requests/RequestQueue.hpp"
#include "dht/HashTableServer.hpp"
#include "dht/HashTableNetworkLayout.hpp"
#include "util/FreeListAllocator.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <zmq.hpp>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unistd.h>
#include <unordered_map>
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
void runCode(util::TestHarness &testHarness)
{
   const uint64_t kEntriesPerHost = 8 * 1024 * 1024;
   const uint64_t kInserts = 1024 * 1024;

   // 1. Start Server
   // Allocate and pin rdma enabled remote memory regions
   // Start zmq socket (REP), which can be used by the clients to retrieve information about memory regions
   dht::HashTableServer localHashTableServer(testHarness.network, kEntriesPerHost, kInserts + 1);
   localHashTableServer.startAddressServiceAsync(testHarness.context, util::getHostname(), 8222);
   if (getenv("VERBOSE"))
      localHashTableServer.dumpMemoryRegions();

   // 2. Network info
   // Create vector with containing node identifiers for all nodes, which host a part of the distributed hash table
   vector<dht::HashTableLocation> hashTableLocations;
   for (uint i = 0; i<testHarness.peerInfos.size(); ++i) {
      dht::HashTableLocation hashTableLocation = {(int) i, testHarness.queuePairs[i].get(), testHarness.peerInfos[i].hostname, 8222};
      hashTableLocations.push_back(hashTableLocation);
   }
   dht::HashTableNetworkLayout hashTableNetworkLayout(hashTableLocations);
   hashTableNetworkLayout.retrieveRemoteMemoryRegions(testHarness.context);
   hashTableNetworkLayout.setupRequestQueues(testHarness.network, 4, 4);
   if (getenv("VERBOSE"))
      hashTableNetworkLayout.dump();

   // 3. Client
   // Connect zmq (REQ) socket to each node and retrieve shared memory regions
   dht::HashTableClient distributedHashTableClient(testHarness.network, hashTableNetworkLayout, localHashTableServer, testHarness.localId, kEntriesPerHost);
   if (getenv("VERBOSE"))
      distributedHashTableClient.dump();

   // 4. Test
   auto begin = chrono::high_resolution_clock::now();
   srand(testHarness.localId + 123);
   for (uint64_t j = 0; j<kInserts; ++j) {
      distributedHashTableClient.insert({(uint64_t) rand(), {0xdeadbeef}});
   }
   for (uint k = 0; k<testHarness.nodeCount; ++k) {
      hashTableNetworkLayout.requestQueues[k]->finishAllOpenRequests();
   }
   auto end = chrono::high_resolution_clock::now();
   cout << "Time for " << kInserts << ": " << chrono::duration_cast<chrono::microseconds>(end - begin).count() << "us" << endl;

   // 5. Done
   cout << "[PRESS ENTER TO PRINT HT]" << endl;
   cin.get();
   if (getenv("VERBOSE")) {
      //      localHashTableServer.dumpHashTableContent(hashTableNetworkLayout);
   }
   //   for (uint64_t j = 0; j<kInserts; ++j) {
   //      uint32_t my_result = distributedHashTableClient.count(j);
   //
   //      if (my_result != testHarness.nodeCount)
   //         cout << "key: " << j << ": " << my_result << " bucket: " << (j % 32) << endl;
   //      //      assert(my_result == ref_result * 2);
   //   }
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();

   // 6. Shutdown
   localHashTableServer.stopAddressService();
}
//---------------------------------------------------------------------------
int main(int argc, char **argv)
{
   // Parse input
   if (argc != 3) {
      cerr << "usage: " << argv[0] << " [nodeCount] [coordinator]" << endl;
      exit(EXIT_FAILURE);
   }
   uint32_t nodeCount = atoi(argv[1]);
   string coordinatorName = argv[2];

   // Create Network/**/
   zmq::context_t context(1);
   rdma::Network network;
   util::TestHarness testHarness(context, network, nodeCount, coordinatorName);
   testHarness.createFullyConnectedNetwork();

   // Run performance tests
   runCode(testHarness);
   testHarness.shutdown();
   context.close();
}
//---------------------------------------------------------------------------
