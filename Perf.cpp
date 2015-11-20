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
int64_t runOneTest(const RemoteMemoryRegion &rmr, const MemoryRegion &sharedMR, QueuePair &queuePair, int bundleSize, int maxBundles, const int iterations, const vector<uint64_t> &randomIndexes);
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
void runServerCode(util::TestHarness &testHarness)
{
   // Create memory
   vector<uint64_t> shared(remoteMemorySize / sizeof(uint64_t)); // PIN complete memory
   fill(shared.begin(), shared.end(), 0);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), testHarness.network.getProtectionDomain(), MemoryRegion::Permission::All);
   for (uint64_t i = 0; i<shared.size(); ++i) {
      shared[i] = i;
   }

   // Publish address
   RemoteMemoryRegion rmr{reinterpret_cast<uintptr_t>(sharedMR.address), sharedMR.key->rkey};
   testHarness.publishAddress(rmr);
   testHarness.retrieveAddress();

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
   cout << "data: " << shared[0] << endl;
}
//---------------------------------------------------------------------------
void runClientCodeNonChained(util::TestHarness &testHarness)
{
   // Get target memory
   RemoteMemoryRegion rmr = testHarness.retrieveAddress();

   // Pin local memory
   vector<uint64_t> shared(1);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), testHarness.network.getProtectionDomain(), MemoryRegion::Permission::All);
   rdma::QueuePair &queuePair = *testHarness.queuePairs[0];

   // Config
   const int kTotalRequests = 1 << 20;
   const int kRuns = 10;
   const int kMaxBundles = 8;
   const int bundleSize = 4;
   const int maxBundles = 4;
   const int kAllowedOutstandingCompletionsByHardware = 16;
   assert(kMaxBundles<kAllowedOutstandingCompletionsByHardware); // and kMaxBundleSize should be a power of two
   cout << "kTotalRequests=" << kTotalRequests << endl;
   cout << "kMaxBundleSize=" << kMaxBundles << endl;
   cout << "kAllowedOutstandingCompletionsByHardware=" << kAllowedOutstandingCompletionsByHardware << endl;

   for (auto memorySize : memorySizes) {
      // Print config
      cout << "memorySize=" << memorySize << endl;
      cout << "bundleSize=" << bundleSize << endl;
      cout << "maxBundles=" << maxBundles << endl;
      cout << "maxOutstandingMessages=" << maxBundles * bundleSize << endl;
      vector<uint64_t> randomIndexes = generateRandomIndexes(kTotalRequests, memorySize, sizeof(uint64_t));

      // Run ten times to get accurate measurements
      vector<int64_t> results;
      for (int run = 0; run<kRuns; run++) {
         int64_t time = runOneTest(rmr, sharedMR, queuePair, bundleSize, maxBundles, kTotalRequests, randomIndexes);
         results.push_back(time);
      }

      // Print results
      cout << "checkSum=" << shared[0] + 1 << endl;
      cout << "times=" << endl;
      sort(results.begin(), results.end());
      for (auto result : results) {
         cout << result << endl;
      }
   }

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
   cout << "data: " << shared[0] << endl;
}
//---------------------------------------------------------------------------
int64_t runOneTest(const RemoteMemoryRegion &rmr, const MemoryRegion &sharedMR, QueuePair &queuePair, const int bundleSize, const int maxBundles, const int kTotalRequests, const vector<uint64_t> &randomIndexes)
{
   // Create work requests
   ReadWorkRequest workRequest;
   workRequest.setId(8028);
   workRequest.setLocalAddress(sharedMR);
   workRequest.setCompletion(false);

   // Track number of outstanding completions
   int openBundles = 0;
   int currentRandomNumber = 0;
   const int requiredBundles = kTotalRequests / bundleSize;

   // Performance
   auto begin = chrono::steady_clock::now();
   for (int i = 0; i<requiredBundles; ++i) {
      workRequest.setCompletion(false);
      for (int b = 0; b<bundleSize - 1; ++b) {
         workRequest.setRemoteAddress(RemoteMemoryRegion{rmr.address + randomIndexes[currentRandomNumber++], rmr.key});
         queuePair.postWorkRequest(workRequest);
      }
      workRequest.setCompletion(true);
      workRequest.setRemoteAddress(RemoteMemoryRegion{rmr.address + randomIndexes[currentRandomNumber++], rmr.key});
      queuePair.postWorkRequest(workRequest);
      openBundles++;

      if (openBundles == maxBundles) {
         queuePair.getCompletionQueuePair().pollSendCompletionQueueBlocking();
         openBundles--;
      }
   }
   while (openBundles != 0) {
      queuePair.getCompletionQueuePair().pollSendCompletionQueueBlocking();
      openBundles--;
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
   uint32_t nodeCount = atoi(argv[1]);
   string coordinatorName = argv[2];

   // Create Network
   zmq::context_t context(1);
   rdma::Network network;
   util::TestHarness testHarness(context, network, nodeCount, coordinatorName);
   testHarness.createFullyConnectedNetwork();

   // Run performance tests
   if (testHarness.localId == 0) {
      runServerCode(testHarness);
   } else {
      runClientCodeNonChained(testHarness);
   }
}
//--