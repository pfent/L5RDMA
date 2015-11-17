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
#include <memory>
#include <array>
#include <vector>
#include <iostream>
#include <zmq.hpp>
//---------------------------------------------------------------------------
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "util/InlineList.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
#include "Request.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct HashTableNetworkLayout;
struct HashTableServer;
struct Request;
//---------------------------------------------------------------------------
class RequestQueue : public util::NotAssignable {
public:
   RequestQueue(uint bundleSize, uint bundleCount, rdma::QueuePair &queuePair, DummyRequest &dummyRequest);

   ~RequestQueue();

   void submit(Request *request);
   void finishAllOpenRequests();

private:
   void send(Request *request);

   struct Bundle {
      std::vector<Request *> requests;
   };

   rdma::QueuePair &queuePair;
   std::vector<Bundle> bundles;
   const uint32_t bundleSize;
   const uint32_t bundleCount;
   uint32_t currentBundle;
   uint32_t nextWorkRequestInBundle;
   uint32_t bundleUpForCompletion;
   DummyRequest &dummyRequest;

   uint64_t sentDummyCount;
   uint64_t sentRequestCount;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
