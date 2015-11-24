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
#include "rdma/QueuePair.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "util/ConnectionSetup.hpp"
#include "util/Peer.hpp"
#include "util/Utility.hpp"
#include "util/MemoryRef.hpp"
#include "util/Coordinator.hpp"
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
#include <random>
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
const uint64_t KB = 1024;
const uint64_t MB = 1024 * KB;
const uint64_t GB = 1024 * MB;
const uint64_t remoteMemorySize = 32 * GB;
vector<uint64_t> memorySizes{1 * KB, 10 * KB, 100 * KB, 1 * MB, 10 * MB, 100 * MB, 1 * GB, 2 * GB, 4 * GB, 8 * GB, 16 * GB, 32 * GB};
//---------------------------------------------------------------------------
// Config
const int kTotalRequests = 1 << 20;
const int kRuns = 10;
const int kMaxBundles = 8;
const int bundleSize = 4;
const int maxBundles = 4;
const int kAllowedOutstandingCompletionsByHardware = 16;
vector<unique_ptr<rdma::QueuePair>> queuePairs;
//---------------------------------------------------------------------------
int64_t runOneTest(const RemoteMemoryRegion &rmr, const MemoryRegion &sharedMR, int bundleSize, int maxBundles, const int iterations, const vector<uint64_t> &randomIndexes);
//---------------------------------------------------------------------------
vector<uint64_t> generateRandomIndexes(uint64_t count, uint64_t max, uint64_t sizeOfType)
{
   std::mt19937 gen(123);
   std::uniform_int_distribution<uint64_t> dis(0, (max / sizeOfType) - 1);

   vector<uint64_t> result(count);
   for (uint i = 0; i<count; ++i) {
      result[i] = sizeOfType * dis(gen);
   }
   return move(result);
}
//---------------------------------------------------------------------------
void createAndShareQueuePairs(util::Peer &peer, rdma::Network &network, uint32_t queuePairCount)
{
   assert(peer.getNodeCount() == 2);

   vector<rdma::Address> addresses;
   for (uint i = 0; i<queuePairCount; ++i) {
      auto completionQueue = new rdma::CompletionQueuePair(network); // TODO leak
      queuePairs.push_back(unique_ptr<rdma::QueuePair>(new rdma::QueuePair(network, *completionQueue)));
      addresses.push_back(rdma::Address{network.getLID(), queuePairs[i]->getQPN()});
   }

   peer.publish("queuePairs", util::MemoryRef((char *) addresses.data(), addresses.size() * sizeof(rdma::Address)));
   peer.barrier();

   vector<uint8_t> remoteAddresses = peer.lookUp("queuePairs", (peer.getLocalId() + 1) % peer.getNodeCount());
   rdma::Address *addressPtr = (rdma::Address *) remoteAddresses.data();
   for (uint i = 0; i<queuePairCount; ++i) {
      queuePairs[i]->connect(addressPtr[i]);
   }
   peer.barrier();
}
//---------------------------------------------------------------------------
void runServerCode(util::Peer &peer, rdma::Network &network)
{
   createAndShareQueuePairs(peer, network, 4);

   for (auto memorySize : memorySizes) {
      // Create memory
      cout << "> Ping " << memorySize << " Bytes" << endl;
      vector<uint64_t> shared(memorySize / sizeof(uint64_t));
      fill(shared.begin(), shared.end(), 0);
      MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

      // Publish address
      RemoteMemoryRegion rmr{reinterpret_cast<uintptr_t>(sharedMR.address), sharedMR.key->rkey};
      peer.publish(util::to_string(memorySize), util::MemoryRef((char *) &rmr, sizeof(RemoteMemoryRegion)));
      peer.barrier();
      peer.barrier();
   }

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
}
//---------------------------------------------------------------------------
void runClientCode(util::Peer &peer, rdma::Network &network, uint32_t serverId)
{
   createAndShareQueuePairs(peer, network, 4);

   // Pin local memory
   vector<uint64_t> shared(1);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

   assert(kMaxBundles<kAllowedOutstandingCompletionsByHardware); // and kMaxBundleSize should be a power of two
   cout << "kTotalRequests=" << kTotalRequests << endl;
   cout << "kMaxBundleSize=" << kMaxBundles << endl;
   cout << "kAllowedOutstandingCompletionsByHardware=" << kAllowedOutstandingCompletionsByHardware << endl;

   for (auto memorySize : memorySizes) {
      // Get target memory
      peer.barrier(); // Wait till info is public
      RemoteMemoryRegion rmr = *(RemoteMemoryRegion *) peer.lookUp(util::to_string(memorySize), serverId).data();

      // Print config
      cout << "memorySize=" << memorySize << endl;
      cout << "bundleSize=" << bundleSize << endl;
      cout << "maxBundles=" << maxBundles << endl;
      cout << "maxOutstandingMessages=" << maxBundles * bundleSize << endl;
      vector<uint64_t> randomIndexes = generateRandomIndexes(kTotalRequests, memorySize, sizeof(uint64_t));

      // Run ten times to get accurate measurements
      vector<int64_t> results;
      for (int run = 0; run<kRuns; run++) {
         int64_t time = runOneTest(rmr, sharedMR, bundleSize, maxBundles, kTotalRequests, randomIndexes);
         results.push_back(time);
      }

      // Print results
      cout << "checkSum=" << shared[0] + 1 << endl;
      cout << "times=" << endl;
      sort(results.begin(), results.end());
      for (auto result : results) {
         cout << result << endl;
      }
      peer.barrier(); // Signal that we are done
   }

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
   cout << "data: " << shared[0] << endl;
}
//---------------------------------------------------------------------------
int64_t runOneTest(const RemoteMemoryRegion &rmr, const MemoryRegion &sharedMR, const int bundleSize, const int maxBundles, const int kTotalRequests, const vector<uint64_t> &randomIndexes)
{
   // Create work requests
   ReadWorkRequest workRequest;
   workRequest.setId(8028);
   workRequest.setLocalAddress(sharedMR);
   workRequest.setCompletion(false);

   // Track number of outstanding completions
   vector<int> openBundles(4, 0);
   int currentRandomNumber = 0;
   const int requiredBundles = kTotalRequests / bundleSize;

   // Performance
   auto begin = chrono::steady_clock::now();
   for (int i = 0; i<requiredBundles; ++i) {
      workRequest.setCompletion(false);
      for (int b = 0; b<bundleSize - 1; ++b) {
         workRequest.setRemoteAddress(RemoteMemoryRegion{rmr.address + randomIndexes[currentRandomNumber++], rmr.key});
         queuePairs[i & 2]->postWorkRequest(workRequest);
      }
      workRequest.setCompletion(true);
      workRequest.setRemoteAddress(RemoteMemoryRegion{rmr.address + randomIndexes[currentRandomNumber++], rmr.key});
      queuePairs[i & 2]->postWorkRequest(workRequest);
      openBundles[i & 2]++;

      if (openBundles[i & 2] == maxBundles) {
         queuePairs[i & 2]->getCompletionQueuePair().pollSendCompletionQueueBlocking();
         openBundles[i & 2]--;
      }
   }
   for (int i = 0; i<4; ++i) {
      while (openBundles[i] != 0) {
         queuePairs[i]->getCompletionQueuePair().pollSendCompletionQueueBlocking();
         openBundles[i]--;
      }
   }

   // Track time
   auto end = chrono::steady_clock::now();
   return chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
}
//---------------------------------------------------------------------------
int main(int argc, char **argv)
{
   // Parse input
   if (argc != 3) {
      cerr << "usage: " << argv[0] << " [nodeCount] [coordinator]" << endl;
      exit(EXIT_FAILURE);
   }
   uint32_t serverId = 0;
   uint32_t nodeCount = atoi(argv[1]);
   string coordinatorName = argv[2];

   // Create Network
   zmq::context_t context(1);
   rdma::Network network;
   util::Peer peer(context, nodeCount, coordinatorName);
   peer.startPublisherService();
   peer.exchangeHostnames();

   // Run performance tests
   if (peer.getLocalId() == serverId) {
      runServerCode(peer, network);
   } else {
      runClientCode(peer, network, serverId);
   }
}
//---------------------------------------------------------------------------