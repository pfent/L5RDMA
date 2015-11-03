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
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct HashTableNetworkLayout;
struct HashTableServer;
//---------------------------------------------------------------------------
class HashTableClient : public util::NotAssignable {
public:
   HashTableClient(rdma::Network &network, HashTableNetworkLayout &remoteTables, HashTableServer &localServer, uint64_t localHostId, uint64_t entryCountPerHost);
   ~HashTableClient();

   void insert(const Entry &entry);
   uint32_t count(uint64_t key) const;

   void dump() const;

private:
   rdma::Network &network;
   HashTableNetworkLayout &remoteTables;
   HashTableServer &localServer;

   const uint64_t localHostId;

   const uint64_t hostCount;
   const uint64_t entryCountPerHost;
   const uint64_t totalEntryCount;

   const uint64_t maskForPositionSelection; // key & maskForPositionSelection = the index in the ht
   const uint64_t maskForHostSelection; // key & maskForHostSelection >> shiftForHostSelection = the id of the host where the ht resides
   const uint64_t shiftForHostSelection; // key & maskForHostSelection >> shiftForHostSelection = the id of the host where the ht resides
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
