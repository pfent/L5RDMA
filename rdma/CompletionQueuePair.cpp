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
#include "CompletionQueuePair.hpp"
#include "WorkRequest.hpp"
#include "Network.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <cstring>
#include <iostream>
#include <iomanip>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
namespace {
//---------------------------------------------------------------------------
string stringForCompletionCode(int opcode)
/// Lookup the type of the completion event
{
   string description;
   switch (opcode) {
      case IBV_WC_RECV:
         description = "IBV_WC_RECV";
         break;
      case IBV_WC_SEND:
         description = "IBV_WC_SEND";
         break;
      case IBV_WC_RDMA_WRITE:
         description = "IBV_WC_RDMA_WRITE";
         break;
      case IBV_WC_RDMA_READ:
         description = "IBV_WC_RDMA_READ";
         break;
      case IBV_WC_COMP_SWAP:
         description = "IBV_WC_COMP_SWAP";
         break;
      case IBV_WC_FETCH_ADD:
         description = "IBV_WC_FETCH_ADD";
         break;
      case IBV_WC_BIND_MW:
         description = "IBV_WC_BIND_MW";
         break;
      case IBV_WC_RECV_RDMA_WITH_IMM:
         description = "IBV_WC_RECV_RDMA_WITH_IMM";
         break;
   }
   return description;
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
CompletionQueuePair::CompletionQueuePair(Network &network)
{
   // Create event channel
   channel = ::ibv_create_comp_channel(network.context);
   if (channel == nullptr) {
      string reason = "creating the completion channel failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create completion queues
   sendQueue = ::ibv_create_cq(network.context, Network::CQ_SIZE, nullptr, channel, 0);
   if (sendQueue == nullptr) {
      string reason = "creating the send completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   receiveQueue = ::ibv_create_cq(network.context, Network::CQ_SIZE, nullptr, channel, 0);
   if (receiveQueue == nullptr) {
      string reason = "creating the receive completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Request notifications
   int status = ::ibv_req_notify_cq(sendQueue, 0);
   if (status != 0) {
      string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   status = ::ibv_req_notify_cq(receiveQueue, 0);
   if (status != 0) {
      string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
CompletionQueuePair::~CompletionQueuePair()
{
   int status;

   // Destroy the completion queues
   status = ::ibv_destroy_cq(sendQueue);
   if (status != 0) {
      string reason = "destroying the send completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   status = ::ibv_destroy_cq(receiveQueue);
   if (status != 0) {
      string reason = "destroying the receive completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Destroy the completion channel
   status = ::ibv_destroy_comp_channel(channel);
   if (status != 0) {
      string reason = "destroying the completion channel failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollCompletionQueue(ibv_cq *completionQueue, int type)
/// Poll a completion queue
{
   int status;

   // Poll for a work completion
   ibv_wc completion;
   status = ::ibv_poll_cq(completionQueue, 1, &completion);
   if (status == 0) {
      return numeric_limits<uint64_t>::max();
   }

   // Check status and opcode
   if (completion.status == IBV_WC_SUCCESS) {
      if (completion.opcode == type) {
         return completion.wr_id;
      } else {
         string reason = "unexpected completion opcode (" + stringForCompletionCode(completion.opcode) + ")";
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   } else {
      string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
pair<bool, uint64_t> CompletionQueuePair::waitForCompletion(bool restricted, bool onlySend)
/// Wait for a work completion
{
   unique_lock <mutex> lock(guard);
   int status;

   // We have to empty the completion queue and cache additional completions
   // as events are only generated when new work completions are enqueued.

   pair<bool, uint64_t> workCompletion;
   bool found = false;

   for (unsigned c = 0; c != cachedCompletions.size(); ++c) {
      if (!restricted || cachedCompletions[c].first == onlySend) {
         workCompletion = cachedCompletions[c];
         cachedCompletions.erase(cachedCompletions.begin() + c);
         found = true;
         break;
      }
   }

   while (!found) {
      // Wait for completion queue event
      ibv_cq *event;
      void *ctx;
      status = ::ibv_get_cq_event(channel, &event, &ctx);
      if (status != 0) {
         string reason = "receiving the completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
         cerr << reason << endl;
         throw NetworkException(reason);
      }
      ::ibv_ack_cq_events(event, 1);
      bool isSendCompletion = (event == sendQueue);

      // Request a completion queue event
      status = ::ibv_req_notify_cq(event, 0);
      if (status != 0) {
         string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
         cerr << reason << endl;
         throw NetworkException(reason);
      }

      // Poll all work completions
      ibv_wc completion;
      do {
         status = ::ibv_poll_cq(event, 1, &completion);

         if (status<0) {
            string reason = "failed to poll completions";
            cerr << reason << endl;
            throw NetworkException(reason);
         }
         if (status == 0) {
            continue;
         }

         if (completion.status != IBV_WC_SUCCESS) {
            string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
            cerr << reason << endl;
            throw NetworkException(reason);
         }

         // Add completion
         if (!found && (!restricted || isSendCompletion == onlySend)) {
            workCompletion = make_pair(isSendCompletion, completion.wr_id);
            found = true;
         } else {
            cachedCompletions.push_back(make_pair(isSendCompletion, completion.wr_id));
         }
      } while (status);
   }

   // Return the oldest completion
   return workCompletion;
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollSendCompletionQueue()
/// Poll the send completion queue
{
    int status;

    // Poll for a work completion
   ibv_wc completion{};
    status = ::ibv_poll_cq(sendQueue, 1, &completion);
    if (status == 0) {
        return numeric_limits<uint64_t>::max();
    }

    // Check status and opcode
    if (completion.status == IBV_WC_SUCCESS) {
        return completion.wr_id;
    } else {
        string reason = "unexpected completion status " + to_string(completion.status) + ": " +
                        ibv_wc_status_str(completion.status);
        cerr << reason << endl;
        throw NetworkException(reason);
    }
}

    uint64_t CompletionQueuePair::pollSendCompletionQueue(int type) {
        return pollCompletionQueue(sendQueue, type);
    }
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollRecvCompletionQueue()
/// Poll the receive completion queue
{
   return pollCompletionQueue(receiveQueue, IBV_WC_RECV);
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollCompletionQueueBlocking(ibv_cq *completionQueue, int type)
/// Poll a completion queue blocking
{
   int status;

   // Poll for a work completion
   ibv_wc completion;
   status = ::ibv_poll_cq(completionQueue, 1, &completion);
   while (status == 0) {
      status = ::ibv_poll_cq(completionQueue, 1, &completion);
   }

   // Check status and opcode
   if (completion.status == IBV_WC_SUCCESS) {
      if (completion.opcode == type) {
         return completion.wr_id;
      } else {
         string reason = "unexpected completion opcode (" + stringForCompletionCode(completion.opcode) + ")";
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   } else {
      string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollSendCompletionQueueBlocking()
/// Poll the send completion queue blocking
{
   return pollCompletionQueueBlocking(sendQueue, IBV_WC_RDMA_READ);
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::pollRecvCompletionQueueBlocking()
/// Poll the receive completion queue blocking
{
   return pollCompletionQueueBlocking(receiveQueue, IBV_WC_RECV);
}
//---------------------------------------------------------------------------
pair<bool, uint64_t> CompletionQueuePair::waitForCompletion()
/// Wait for a work completion
{
   return waitForCompletion(false, false);
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::waitForCompletionSend()
/// Wait for a work completion
{
   return waitForCompletion(true, true).second;
}
//---------------------------------------------------------------------------
uint64_t CompletionQueuePair::waitForCompletionReceive()
/// Wait for a work completion
{
   return waitForCompletion(true, false).second;
}
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
