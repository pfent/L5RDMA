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
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "util/NotAssignable.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct RemoteMemoryRegion;
struct MemoryRegion;
//---------------------------------------------------------------------------
/// Identifies the host of a hash table
struct HashTableLocation {
   int qpIndex; // Should be the same position as in the vector in the network class ?
   std::string hostname;
   int port;
};
//---------------------------------------------------------------------------
struct Entry {
   uint64_t key;
   std::array<uint64_t, 1> payload;
};
//---------------------------------------------------------------------------
struct BucketLocator {
   BucketLocator(uint64_t host, uint64_t offset)
           : data((host << 48) | offset)
   {
      assert(host<(1ull << 16));
      assert(offset<(1ull << 48));
   }
   BucketLocator()
           : data(-1) { }

   uint64_t getHost() const { return data >> 48; } // Upper 16 bit
   uint64_t getOffset() const { return data & ((1ull << 48) - 1); } // Lower 48 bit

   uint64_t data;
};
static_assert(sizeof(BucketLocator) == sizeof(uint64_t), ""); // TODO: is sizeof(uint64_t) stupid ?
//---------------------------------------------------------------------------
struct Bucket {
   Entry entry;
   BucketLocator next;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
