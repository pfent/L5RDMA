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
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
void runServerCode(util::TestHarness &testHarness)
{
   // Create memory
   vector<uint64_t> shared(128);
   fill(shared.begin(), shared.end(), 0);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), testHarness.network.getProtectionDomain(), MemoryRegion::Permission::All);

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
void runClientCodeChained(util::TestHarness &testHarness)
{
   // Get target memory
   RemoteMemoryRegion rmr = testHarness.retrieveAddress();

   // Pin local memory
   vector<uint64_t> shared(1);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), testHarness.network.getProtectionDomain(), MemoryRegion::Permission::All);
   rdma::QueuePair &queuePair = *testHarness.queuePairs[0];

   const int maxOpenCompletions = 1;
   int openCompletions = 0;

   for (int warmUp = 0; warmUp<10; warmUp++) {
      for (int run = 0; run<=0; run++) {
         const int bundleSize = (1 << run);
         const int totalRequests = 1 << 20;
         const int iterations = totalRequests / bundleSize;

         // Create work requests
         vector<unique_ptr<AtomicFetchAndAddWorkRequest>> workRequests(bundleSize);
         for (int i = 0; i<bundleSize; ++i) {
            workRequests[i] = make_unique<AtomicFetchAndAddWorkRequest>();
            workRequests[i]->setId(8028 + i);
            workRequests[i]->setLocalAddress(sharedMR);
            workRequests[i]->setRemoteAddress(rmr);
            workRequests[i]->setAddValue(1);
            workRequests[i]->setCompletion(i == bundleSize - 1);
            if (i != 0)
               workRequests[i - 1]->setNextWorkRequest(workRequests[i].get());
         }

         // Performance
         auto begin = chrono::high_resolution_clock::now();
         for (int i = 0; i<iterations; ++i) {
            queuePair.postWorkRequest(*workRequests[0]);
            openCompletions++;

            if (openCompletions == maxOpenCompletions) {
               queuePair.getCompletionQueuePair().waitForCompletionSend();
               openCompletions--;
            }
         }
         while (openCompletions != 0) {
            queuePair.getCompletionQueuePair().waitForCompletionSend();
            openCompletions--;
         }
         auto end = chrono::high_resolution_clock::now();

         cout << "run: " << run << endl;
         cout << "total: " << totalRequests << endl;
         cout << "iterations: " << iterations << endl;
         cout << "bundleSize: " << bundleSize << endl;
         cout << "maxOpenCompletions: " << maxOpenCompletions << endl;
         cout << "time: " << chrono::duration_cast<chrono::nanoseconds>(end - begin).count() << endl;
         cout << "data: " << shared[0] + 1 << endl;
         cout << "---" << endl;
      }
   }

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

   const int maxOpenCompletions = 1;
   int openCompletions = 0;

   for (int warmUp = 0; warmUp<10; warmUp++) {
      for (int run = 1; run<=1; run++) {
         const int bundleSize = (1 << run);
         const int totalRequests = 1 << 20;
         const int iterations = totalRequests / bundleSize;

         // Create work requests
         AtomicFetchAndAddWorkRequest workRequest;
         workRequest.setId(8028);
         workRequest.setLocalAddress(sharedMR);
         workRequest.setRemoteAddress(rmr);
         workRequest.setAddValue(1);
         workRequest.setCompletion(false);

         // Performance
         auto begin = chrono::high_resolution_clock::now();
         for (int i = 0; i<iterations; ++i) {
            workRequest.setCompletion(false);
            for (int b = 0; b<bundleSize - 1; ++b)
               queuePair.postWorkRequest(workRequest);
            workRequest.setCompletion(true);
            queuePair.postWorkRequest(workRequest);
            openCompletions++;

            if (openCompletions == maxOpenCompletions) {
               queuePair.getCompletionQueuePair().waitForCompletionSend();
               openCompletions--;
            }
         }
         while (openCompletions != 0) {
            queuePair.getCompletionQueuePair().waitForCompletionSend();
            openCompletions--;
         }
         auto end = chrono::high_resolution_clock::now();

         cout << "run: " << run << endl;
         cout << "total: " << totalRequests << endl;
         cout << "iterations: " << iterations << endl;
         cout << "bundleSize: " << bundleSize << endl;
         cout << "maxOpenCompletions: " << maxOpenCompletions << endl;
         cout << "ttime: " << chrono::duration_cast<chrono::nanoseconds>(end - begin).count() << endl;
         cout << "data: " << shared[0] + 1 << endl;
         cout << "---" << endl;
      }
   }

   // Done
   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();
   cout << "data: " << shared[0] << endl;
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
