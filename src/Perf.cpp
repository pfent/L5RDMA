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
#include <infiniband/verbs.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unistd.h>
#include <zmq.hpp>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "rdma/MemoryRegion.hpp"
#include "rdma/WorkRequest.hpp"
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
struct TestHarness {

   Network network;
   uint32_t localId;
   uint32_t nodeCount;
   string coordinatorHostName;
   zmq::context_t context;
   bool verbose;
   unique_ptr <zmq::socket_t> masterSocket;
   unique_ptr <zmq::socket_t> broadcastSocket;

   TestHarness(uint32_t nodeCount, string coordinatorHostName) : network(nodeCount), localId(-1), nodeCount(nodeCount), coordinatorHostName(coordinatorHostName), context(1), verbose(getenv("VERBOSE"))
   {
      // Request reply socket
      masterSocket = make_unique<zmq::socket_t>(context, ZMQ_REQ);
      masterSocket->connect(("tcp://" + coordinatorHostName + ":8028").c_str());

      // Broadcast socket
      broadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_SUB);
      broadcastSocket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
      broadcastSocket->connect(("tcp://" + coordinatorHostName + ":8029").c_str());
   }

   // requires a coordinator on [HOSTNAME] running "supportFullyConnectedNetworkCreation"
   void createFullyConnectedNetwork()
   {
      // Create RDMA QPs
      vector <Address> localAddresses;
      for (uint32_t i = 0; i<nodeCount; ++i) {
         localAddresses.push_back(Address{network.getLID(), network.getQPN(i)});
         cout << network.getLID() << " " << network.getQPN(i) << endl;
      }
      if (verbose)
         cout << "> Created RDMA Network" << endl;

      // Send address
      zmq::message_t request(nodeCount * sizeof(Address));
      assert(localAddresses.size() * sizeof(Address) == request.size());
      memcpy(request.data(), localAddresses.data(), request.size());
      masterSocket->send(request);
      if (verbose)
         cout << "> Sent qp addresses" << endl;

      // Receive Id
      zmq::message_t reply;
      masterSocket->recv(&reply);
      assert(reply.size() == sizeof(uint32_t));
      localId = *reinterpret_cast<uint32_t *>(reply.data());
      if (verbose)
         cout << "> Local id: " << localId << endl;

      // Receive addresses of other clients
      zmq::message_t allAddresses(nodeCount * nodeCount * sizeof(Address));
      broadcastSocket->recv(&allAddresses);
      vector <Address> remoteAddresses(nodeCount);
      Address *addressPtr = reinterpret_cast<Address *>(allAddresses.data());
      for (uint32_t i = 0; i<nodeCount; i++) {
         remoteAddresses[i] = addressPtr[i * nodeCount + localId];
         cout << "connecting: " << remoteAddresses[i].lid << " " << remoteAddresses[i].qpn << endl;
      }
      network.connect(remoteAddresses);
      if (verbose)
         cout << ">> Done" << endl;
   }

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   void publishAddress(RemoteMemoryRegion &remoteMemoryRegion)
   {
      // Send address
      if (verbose)
         cout << "> Publish Remote Address: key=" << remoteMemoryRegion.key << " address=" << remoteMemoryRegion.address << endl;
      zmq::message_t request(sizeof(RemoteMemoryRegion));
      *reinterpret_cast<RemoteMemoryRegion *>(request.data()) = remoteMemoryRegion;
      masterSocket->send(request);
      masterSocket->recv(&request); // Does not matter
      if (verbose)
         cout << ">> Done" << endl;
   }

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   RemoteMemoryRegion retrieveAddress()
   {
      // Create TCP Client
      if (verbose)
         cout << "> Retrieve Remote Address" << endl;

      // Receive address
      zmq::message_t request(sizeof(RemoteMemoryRegion));
      broadcastSocket->recv(&request);
      RemoteMemoryRegion remoteMemoryRegion = *reinterpret_cast<RemoteMemoryRegion *>(request.data());
      if (verbose) {
         cout << "> Remote Address: key=" << remoteMemoryRegion.key << " address=" << remoteMemoryRegion.address << endl;
         cout << ">> Done" << endl;
      }

      return remoteMemoryRegion;
   }
};
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
   TestHarness alex(nodeCount, coordinatorName);
   alex.createFullyConnectedNetwork();

   vector <uint64_t> shared(128);
   fill(shared.begin(), shared.end(), 0);
   MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), alex.network.getProtectionDomain(), MemoryRegion::Permission::All);

   // Exchange memory region address
   if (alex.localId == 0) {
      RemoteMemoryRegion rmr{reinterpret_cast<uintptr_t>(sharedMR.address), sharedMR.key->rkey};
      alex.publishAddress(rmr);
   }
   RemoteMemoryRegion rmr = alex.retrieveAddress();

   // Do performance test
   if (alex.localId == 1) {
      vector <uint64_t> shared(1);
      MemoryRegion sharedMR(shared.data(), sizeof(uint64_t) * shared.size(), alex.network.getProtectionDomain(), MemoryRegion::Permission::All);

      AtomicFetchAndAddWorkRequest request;
      request.setId(8028);
      request.setCompletion(true);
      request.setLocalAddress(sharedMR);
      request.setRemoteAddress(rmr);
      request.setAddValue(1);
//      alex.network.postFetchAdd(0, sharedMR, rmr, 42, true, 8028);
      alex.network.postWorkRequest(0, request);
      alex.network.waitForCompletionSend();

      cout << "previous: " << shared[0] << endl;
   }

   cout << "[PRESS ENTER TO CONTINUE]" << endl;
   cin.get();

   cout << "data: " << shared[0] << endl;
}
