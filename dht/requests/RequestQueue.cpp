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
#include "dht/requests/RequestQueue.hpp"
#include "dht/requests/Request.hpp"
#include "rdma/Network.hpp"
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
RequestQueue::RequestQueue(uint bundleSize, uint bundleCount, rdma::QueuePair &queuePair, DummyRequest &dummyRequest)
        : queuePair(queuePair)
          , bundles(bundleCount, Bundle{vector<Request *>(bundleSize)})
          , bundleSize(bundleSize)
          , bundleCount(bundleCount)
          , currentBundle(0)
          , nextWorkRequestInBundle(0)
          , bundleUpForCompletion(0)
          , dummyRequest(dummyRequest)
          , sentDummyCount(0)
          , sentRequestCount(0)
{
}
//---------------------------------------------------------------------------
RequestQueue::~RequestQueue()
{
   cout << "sentDummyCount = " << sentDummyCount << endl;
   cout << "sentRequestCount = " << sentRequestCount << endl;
}
//---------------------------------------------------------------------------
void RequestQueue::submit(Request *request)
{
   // Wait
   while (nextWorkRequestInBundle>=bundleSize) {
      nextWorkRequestInBundle = 0;
      currentBundle = (currentBundle + 1) % bundleCount;

      // Wait until the bundle is completed
      if (currentBundle == bundleUpForCompletion) {
         // Wait for completion event
         queuePair.getCompletionQueuePair().waitForCompletionSend();
         bundleUpForCompletion = (bundleUpForCompletion + 1) % bundleCount;

         // Notify all request of this bundle that they are completed
         for (auto iter : bundles[currentBundle].requests)
            if (iter->onCompleted() == RequestStatus::SEND_AGAIN)
               send(iter);
      }
   }

   // Send
   send(request);
}
//---------------------------------------------------------------------------
void RequestQueue::finishAllOpenRequests()
{
   // Find all open work request
   std::vector<Request *> openRequests;
   while (nextWorkRequestInBundle != 0 || currentBundle != bundleUpForCompletion) {
      if (nextWorkRequestInBundle == 0) {
         currentBundle = (currentBundle - 1 + bundleCount) % bundleCount;
         nextWorkRequestInBundle = bundleSize;
      } else {
         nextWorkRequestInBundle--;
         openRequests.push_back(bundles[currentBundle].requests[nextWorkRequestInBundle]);
      }
   }

   // Wait for completions
   for (uint i = 0; i<openRequests.size() / bundleSize; ++i)
      queuePair.getCompletionQueuePair().waitForCompletionSend();

   // Make them finish
   while (!openRequests.empty()) {
      sentDummyCount++;
      queuePair.postWorkRequest(dummyRequest.workRequest);
      queuePair.getCompletionQueuePair().waitForCompletionSend();

      std::vector<Request *> stillOpenRequests;
      for (auto iter : openRequests) {
         if (iter->onCompleted() == RequestStatus::SEND_AGAIN) {
            iter->getRequest()->setCompletion(false);
            sentRequestCount++;
            queuePair.postWorkRequest(*iter->getRequest());
            stillOpenRequests.push_back(iter);
         }
      }
      swap(openRequests, stillOpenRequests);
   }
}
//---------------------------------------------------------------------------
void RequestQueue::send(Request *request)
{
   sentRequestCount++;
   bundles[currentBundle].requests[nextWorkRequestInBundle++] = request;
   request->getRequest()->setCompletion(nextWorkRequestInBundle == bundleSize);
   queuePair.postWorkRequest(*request->getRequest());
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
