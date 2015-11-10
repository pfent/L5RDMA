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
#include <zmq.hpp>
#include <thread>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "util/ConnectionSetup.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
namespace rdma {
class QueuePair;
}
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct HashTableNetworkLayout;
//---------------------------------------------------------------------------
struct HashTableServer : public util::NotAssignable {

   HashTableServer(rdma::Network &network, uint32_t htSize, uint32_t maxBucketCount);
   ~HashTableServer();

   void startAddressServiceAsync(zmq::context_t &context, std::string hostname, int port);
   void stopAddressService();

   void dumpMemoryRegions();
   void dumpHashTableContent(HashTableNetworkLayout &hashTableNetworkLayout);

   // Memory itself
   std::vector <BucketLocator> htMemory;
   std::vector <Bucket> bucketMemory;
   uint64_t nextFreeOffset;

private:
   rdma::Network &network;

   // Pinned memory (underlying memory is declared above)
   std::unique_ptr <rdma::MemoryRegion> htMr;
   std::unique_ptr <rdma::MemoryRegion> bucketsMr;
   std::unique_ptr <rdma::MemoryRegion> nextFreeOffsetMr;

   // A socket to distribute the addresses of the pinned memory regions
   std::unique_ptr <zmq::socket_t> socket;
   std::string hostname;
   int port;
   std::unique_ptr <std::thread> thread;
   bool running;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
