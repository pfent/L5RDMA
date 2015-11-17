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
#include "HashTableClient.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/Network.hpp"
#include "rdma/WorkRequest.hpp"
#include "dht/HashTableNetworkLayout.hpp"
#include "util/Utility.hpp"
#include "dht/requests/RequestQueue.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <iostream>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace dht {
//---------------------------------------------------------------------------
HashTableNetworkLayout::HashTableNetworkLayout(const vector<HashTableLocation> &hashTableLocations)
        : locations(hashTableLocations)
          , remoteHashTables(locations.size())
          , requestQueues(locations.size())
{
}
//---------------------------------------------------------------------------
void HashTableNetworkLayout::retrieveRemoteMemoryRegions(zmq::context_t &context)
{
   for (size_t i = 0; i<locations.size(); ++i) {
      zmq::socket_t socket(context, ZMQ_REQ);
      socket.connect(("tcp://" + locations[i].hostname + ":" + util::to_string(locations[i].port)).c_str());

      zmq::message_t request(0);
      socket.send(request);

      zmq::message_t response(sizeof(rdma::RemoteMemoryRegion) * 3);
      socket.recv(&response);

      remoteHashTables[i].htRmr = reinterpret_cast<rdma::RemoteMemoryRegion *>(response.data())[0];
      remoteHashTables[i].bucketsRmr = reinterpret_cast<rdma::RemoteMemoryRegion *>(response.data())[1];
      remoteHashTables[i].nextFreeOffsetRmr = reinterpret_cast<rdma::RemoteMemoryRegion *>(response.data())[2];

      socket.close();
   }
}
//---------------------------------------------------------------------------
void HashTableNetworkLayout::setupRequestQueues(rdma::Network &network, uint bundleSize, uint bundleCount)
{
   for (size_t i = 0; i<locations.size(); ++i) {
      DummyRequest *dummyRequest = new DummyRequest(network, remoteHashTables[i].nextFreeOffsetRmr); // TODO LEAK
      requestQueues[i] = unique_ptr<RequestQueue>(new RequestQueue(network, bundleSize, bundleCount, *locations[i].queuePair, *dummyRequest));
   }
}
//---------------------------------------------------------------------------
void HashTableNetworkLayout::dump()
{
   cout << "> Network Info (" << remoteHashTables.size() << ")" << endl;
   for (size_t i = 0; i<remoteHashTables.size(); ++i) {
      cout << ">   Remote Hash Table (" << i << ")" << endl;
      cout << ">     location= {qpIndex=" << locations[i].qpIndex << " hostname=" << locations[i].hostname << " port=" << locations[i].port << "}" << endl;
      cout << ">     htRmr= {" << remoteHashTables[i].htRmr << "}" << endl;
      cout << ">     bucketsRmr= {" << remoteHashTables[i].bucketsRmr << "}" << endl;
      cout << ">     nextFreeOffsetRmr= {" << remoteHashTables[i].nextFreeOffsetRmr << "}" << endl;
   }
}
//---------------------------------------------------------------------------
} // End of namespace dht
//---------------------------------------------------------------------------
