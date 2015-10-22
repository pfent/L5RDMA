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
#include "Utility.hpp"
#include "ConnectionSetup.hpp"
// ---------------------------------------------------------------------------
#include <iostream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unistd.h>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
TestHarness::TestHarness(zmq::context_t &context, uint32_t nodeCount, const string &coordinatorHostName) : context(context), network(nodeCount), localId(-1), nodeCount(nodeCount), coordinatorHostName(coordinatorHostName), verbose(getenv("VERBOSE"))
{
   // Request reply socket
   masterSocket = make_unique<zmq::socket_t>(context, ZMQ_REQ);
   masterSocket->connect(("tcp://" + coordinatorHostName + ":8028").c_str());

   // Broadcast socket
   broadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_SUB);
   broadcastSocket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
   broadcastSocket->connect(("tcp://" + coordinatorHostName + ":8029").c_str());

   // Wait for setup to finish .. great job zmq
   usleep(100000); // = 100ms
}
//---------------------------------------------------------------------------
// requires a coordinator on [HOSTNAME] running "supportFullyConnectedNetworkCreation"
void TestHarness::createFullyConnectedNetwork()
{
   // Create RDMA QPs
   vector <rdma::Address> localAddresses;
   for (uint32_t i = 0; i<nodeCount; ++i) {
      localAddresses.push_back(rdma::Address{network.getLID(), network.getQPN(i)});
   }
   if (verbose)
      cout << "> Created RDMA Network" << endl;

   // Send address
   zmq::message_t request(nodeCount * sizeof(rdma::Address));
   assert(localAddresses.size() * sizeof(rdma::Address) == request.size());
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
   zmq::message_t allAddresses(nodeCount * nodeCount * sizeof(rdma::Address));
   broadcastSocket->recv(&allAddresses);
   vector <rdma::Address> remoteAddresses(nodeCount);
   rdma::Address *addressPtr = reinterpret_cast<rdma::Address *>(allAddresses.data());
   for (uint32_t i = 0; i<nodeCount; i++) {
      remoteAddresses[i] = addressPtr[i * nodeCount + localId];
      cout << "connecting: " << remoteAddresses[i].lid << " " << remoteAddresses[i].qpn << endl;
   }
   network.connect(remoteAddresses);
   if (verbose)
      cout << ">> Done" << endl;
}
//---------------------------------------------------------------------------
// requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
void TestHarness::publishAddress(rdma::RemoteMemoryRegion &remoteMemoryRegion)
{
   // Send address
   if (verbose)
      cout << "> Publish Remote Address: key=" << remoteMemoryRegion.key << " address=" << remoteMemoryRegion.address << endl;
   zmq::message_t request(sizeof(rdma::RemoteMemoryRegion));
   *reinterpret_cast<rdma::RemoteMemoryRegion *>(request.data()) = remoteMemoryRegion;
   masterSocket->send(request);
   masterSocket->recv(&request); // Does not matter
   if (verbose)
      cout << ">> Done" << endl;
}
//---------------------------------------------------------------------------
// requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
rdma::RemoteMemoryRegion TestHarness::retrieveAddress()
{
   // Create TCP Client
   if (verbose)
      cout << "> Retrieve Remote Address" << endl;

   // Receive address
   zmq::message_t request(sizeof(rdma::RemoteMemoryRegion));
   broadcastSocket->recv(&request);
   rdma::RemoteMemoryRegion remoteMemoryRegion = *reinterpret_cast<rdma::RemoteMemoryRegion *>(request.data());
   if (verbose) {
      cout << "> Remote Address: key=" << remoteMemoryRegion.key << " address=" << remoteMemoryRegion.address << endl;
      cout << ">> Done" << endl;
   }

   return remoteMemoryRegion;
}
//---------------------------------------------------------------------------
SetupSupport::SetupSupport(zmq::context_t &context) : context(context)
{
   // Create sign-up socket
   masterSocket = make_unique<zmq::socket_t>(context, ZMQ_REP);
   masterSocket->bind("tcp://*:8028");

   // Create broadcast socket
   broadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_PUB);
   broadcastSocket->bind("tcp://*:8029");
}
//---------------------------------------------------------------------------
SetupSupport::~SetupSupport()
{
   masterSocket->close();
   broadcastSocket->close();
}
//---------------------------------------------------------------------------
void SetupSupport::supportFullyConnectedNetworkCreation(uint32_t nodeCount)
{
   // Connect to all nodes
   zmq::message_t allAddresses(nodeCount * nodeCount * sizeof(rdma::Address));
   rdma::Address *target = reinterpret_cast<rdma::Address *>(allAddresses.data());
   for (uint32_t i = 0; i<nodeCount; ++i) {
      zmq::message_t request;
      masterSocket->recv(&request);
      memcpy(target + nodeCount * i, request.data(), request.size());

      zmq::message_t reply(sizeof(uint32_t));
      *reinterpret_cast<uint32_t *>(reply.data()) = i;
      masterSocket->send(reply);
   }

   // Broadcast to everyone
   broadcastSocket->send(allAddresses);
}
//---------------------------------------------------------------------------
void SetupSupport::supportRemoteMemoryAddressPublishing()
{
   // Read address from master socket and send to everyone
   zmq::message_t msg(sizeof(rdma::RemoteMemoryRegion));
   masterSocket->recv(&msg);
   cout << "sleeping for 2s before starting clients ... " << endl;
   usleep(2000000);
   cout << "running !!" << endl;
   broadcastSocket->send(msg);
   masterSocket->send(msg); // Does not matter
}
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
