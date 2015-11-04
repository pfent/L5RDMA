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
   const uint64_t kEntriesPerHost = 32;

   // 1. Start Server
   // Allocate and pin rdma enabled remote memory regions
   // Start zmq socket (REP), which can be used by the clients to retrieve information about memory regions
   dht::HashTableServer localHashTableServer(testHarness.network, kEntriesPerHost, 1024 * 1024);
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
   hashTableNetworkLayout.setupRequestQueues(testHarness.network, 1, 1);
   if (getenv("VERBOSE"))
      hashTableNetworkLayout.dump();

   // 3. Client
   // Connect zmq (REQ) socket to each node and retrieve shared memory regions
   dht::HashTableClient distributedHashTableClient(testHarness.network, hashTableNetworkLayout, localHashTableServer, testHarness.localId, kEntriesPerHost);
   if (getenv("VERBOSE"))
      distributedHashTableClient.dump();

   // 4. Test
   if (testHarness.localId == 0) {
      distributedHashTableClient.insert(dht::Entry{2, array<uint64_t, 1>()});
   }
   //   srand(0);
   //   unordered_multimap<uint64_t, uint64_t> reference;
   //   for (int j = 0; j<10; ++j) {
   //      uint64_t key = rand();
   //      distributedHashTableClient.insert({key, {0xdeadbeef}});
   //      reference.insert(make_pair(key, 0xdeadbeef));
   //   }

   // 5. Done
   cout << "[PRESS ENTER TO PRINT HT]" << endl;
   cin.get();
   if (getenv("VERBOSE"))
      localHashTableServer.dumpHashTableContent(hashTableNetworkLayout);
   //   for (auto iter: reference) {
   //      uint32_t my_result = distributedHashTableClient.count(iter.first);
   //      uint32_t ref_result = reference.count(iter.first);
   //      assert(my_result == 2 * ref_result);
   //   }
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
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
   util::TestHarness testHarness(context, nodeCount, coordinatorName);
   testHarness.createFullyConnectedNetwork();

   // Run performance tests
   runCode(testHarness);
}
//---------------------------------------------------------------------------
