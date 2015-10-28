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
#include "dht/HashTableServer.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace dht { // Distributed Hash Table
//---------------------------------------------------------------------------
struct RemoteMemoryRegion;
struct MemoryRegion;
//---------------------------------------------------------------------------
/// Gathers and stores information about remote shared hash tables (locals are treated as remote, because of atomicity (ask alex))
struct HashTableNetworkLayout : public util::NotAssignable {

   HashTableNetworkLayout();
   void retrieveRemoteMemoryRegions(zmq::context_t &context, const std::vector <HashTableLocation> &tableLocations);
   void dump();

   struct RemoteHashTableInfo {
      HashTableLocation location;

      rdma::RemoteMemoryRegion htRmr;
      rdma::RemoteMemoryRegion bucketsRmr;
      rdma::RemoteMemoryRegion nextFreeOffsetRmr;
   };

   std::vector <RemoteHashTableInfo> remoteHashTables;
};
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
