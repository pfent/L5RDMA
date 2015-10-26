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
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
#include "util/ConnectionSetup.hpp"
#include "util/Utility.hpp"
#include "dht/HashTableClient.hpp"
#include "dht/HashTableServer.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unistd.h>
#include <zmq.hpp>
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
void runServerCode(util::TestHarness &testHarness)
{
   // Allocate and pin rdma enabled remote memory regions
   dht::HashTableServer hashTableServer(testHarness.network, 32, 1024 * 1024);

   // Start zmq socket (REP), which can be used by the clients to retrieve information about memory regions
   hashTableServer.startAddressServiceAsync(testHarness.context, util::getHostname(), 8222);
   hashTableServer.dumpMemoryRegions();

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();

   // Dump
   hashTableServer.dumpHashTableContent();
}
//---------------------------------------------------------------------------
void runClientCode(util::TestHarness &testHarness)
{
   // Create vector with containing node identifiers for all nodes, which host a part of the distributed hash table
   int hashTableHostQPId = 0;
   dht::HashTableLocation hashTableLocation = {hashTableHostQPId, testHarness.peerInfos[0].hostname, 8222};
   vector <dht::HashTableLocation> hashTableLocations = {hashTableLocation};

   // Connect zmq (REQ) socket to each node and retrieve shared memory regions
   dht::HashTableNetworkLayout hashTableNetworkLayout;
   hashTableNetworkLayout.retrieveRemoteMemoryRegions(testHarness.context, hashTableLocations);
   hashTableNetworkLayout.dump();

   // Create a hash table client
   hashTableNetworkLayout.remoteHashTables.push_back(hashTableNetworkLayout.remoteHashTables.back());
   hashTableNetworkLayout.remoteHashTables.push_back(hashTableNetworkLayout.remoteHashTables.back());
   hashTableNetworkLayout.remoteHashTables.push_back(hashTableNetworkLayout.remoteHashTables.back());

   dht::HashTableClient distributedHashTableClient(testHarness.network, hashTableNetworkLayout, 32);
//   distributedHashTableClient.insert(dht::Entry{42, {0xdeadbeef}});
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

   // Create Network
   zmq::context_t context(1);
   util::TestHarness testHarness(context, nodeCount, coordinatorName);
   testHarness.createFullyConnectedNetwork();

   // Run performance tests
   if (testHarness.localId == 0) {
      runServerCode(testHarness);
   } else {
      runClientCode(testHarness);
   }
}
//---------------------------------------------------------------------------
