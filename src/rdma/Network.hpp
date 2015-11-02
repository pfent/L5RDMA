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
#pragma once
//---------------------------------------------------------------------------
#include <mutex>
#include <stdexcept>
#include <vector>
#include <memory>
//---------------------------------------------------------------------------
struct ibv_comp_channel;
struct ibv_context;
struct ibv_cq;
struct ibv_device;
struct ibv_mr;
struct ibv_pd;
struct ibv_qp;
struct ibv_srq;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
class WorkRequest;
class MemoryRegion;
class QueuePair;
//---------------------------------------------------------------------------
/// A network exception
class NetworkException : public std::runtime_error {
public:
   NetworkException(const std::string &reason)
           : std::runtime_error(reason) { }
};
//---------------------------------------------------------------------------
struct RemoteMemoryRegion {
   uintptr_t address;
   uint32_t key;
};
std::ostream &operator<<(std::ostream &os, const RemoteMemoryRegion &remoteMemoryRegion);
//---------------------------------------------------------------------------
/// The LID and QPN uniquely address a queue pair
struct Address {
   uint16_t lid;
   uint32_t qpn;
};
std::ostream &operator<<(std::ostream &os, const Address &address);
//---------------------------------------------------------------------------
/// A network of nodes connected via RDMA
class Network {
protected:
   /// The minimal number of entries for the completion queue
   static const int CQ_SIZE = 100;

   /// The port of the Infiniband device
   uint8_t ibport;

   /// The Infiniband devices
   ibv_device **devices;
   /// The verbs context
   ibv_context *context;
   /// The global protection domain
   ibv_pd *protectionDomain;

   /// The shared send completion queue
   ibv_cq *sharedCompletionQueueSend;
   /// The shared receive completion queue
   ibv_cq *sharedCompletionQueueRecv;
   /// The shared completion channel
   ibv_comp_channel *sharedCompletionChannel;
   /// The shared receive queue
   ibv_srq *sharedReceiveQueue;

   /// The cached work completions
   std::vector <std::pair<bool, uint64_t>> cachedCompletions;
   std::mutex completionMutex;

   /// Create queue pair
   ibv_qp *createQueuePair(ibv_cq *cqSend, ibv_cq *cqRecv);
   /// Poll a completion queue
   uint64_t pollCompletionQueue(ibv_cq *completionQueue, int type);

   /// Wait for a work request completion
   std::pair<bool, uint64_t> waitForCompletion(bool restrict, bool onlySend);

   /// Helper function to create a completion queue pair
   void createSharedCompletionQueues();

public:
   /// Constructor
   Network();
   /// Destructor
   ~Network();

   /// Get the LID
   uint16_t getLID();
   /// Create a queue pair with shared receive queues
   std::unique_ptr <QueuePair> createSharedQueuePair();
   void connectQueuePair(QueuePair &queuePair, const Address &address, unsigned retryCount);

   /// Poll the send completion queue
   uint64_t pollSendCompletionQueue();
   /// Poll the receive completion queue
   uint64_t pollRecvCompletionQueue();

   // Poll a completion queue blocking
   uint64_t pollCompletionQueueBlocking(ibv_cq *completionQueue, int type);
   /// Poll the send completion queue blocking
   uint64_t pollSendCompletionQueueBlocking();
   /// Poll the receive completion queue blocking
   uint64_t pollRecvCompletionQueueBlocking();

   /// Wait for a work request completion
   std::pair<bool, uint64_t> waitForCompletion();
   uint64_t waitForCompletionSend();
   uint64_t waitForCompletionReceive();

   /// Get the protection domain
   ibv_pd *getProtectionDomain() { return protectionDomain; }

   /// Print the capabilities of the RDMA host channel adapter
   void printCapabilities();
};
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
