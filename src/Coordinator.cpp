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
//#include <infiniband/verbs.h>
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
namespace {
uint32_t getNodeCount(int argc, char **argv)
{
   if (argc != 2) {
      cerr << "usage: " << argv[0] << " [nodeCount]" << endl;
      exit(EXIT_FAILURE);
   }
   uint32_t nodeCount;
   istringstream in(argv[1]);
   in >> nodeCount;
   return nodeCount;
}
}
//---------------------------------------------------------------------------
struct SetupSupport {
   zmq::context_t context;
   unique_ptr <zmq::socket_t> masterSocket;
   unique_ptr <zmq::socket_t> broadcastSocket;

   SetupSupport() : context(1)
   {
      // Create sign-up socket
      masterSocket = make_unique<zmq::socket_t>(context, ZMQ_REP);
      masterSocket->bind("tcp://*:8028");

      // Create broadcast socket
      broadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_PUB);
      broadcastSocket->bind("tcp://*:8029");
   }

   ~SetupSupport()
   {
      masterSocket->close();
      broadcastSocket->close();
   }

   void supportFullyConnectedNetworkCreation(uint32_t nodeCount)
   {
      // Connect to all nodes
      zmq::message_t allAddresses(nodeCount * nodeCount * sizeof(Address));
      Address *target = reinterpret_cast<Address *>(allAddresses.data());
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

   void supportRemoteMemoryAddressPublishing()
   {
      // Read address from master socket and send to everyone
      zmq::message_t msg(sizeof(RemoteMemoryRegion));
      masterSocket->recv(&msg);
      broadcastSocket->send(msg);
      masterSocket->send(msg); // Does not matter
   }
};
//---------------------------------------------------------------------------
int main(int argc, char **argv)
{
   uint32_t nodeCount = getNodeCount(argc, argv);
   SetupSupport setupSupport;

   while (1) {
      cout << "> Creating FullyConnectedNetworkCreation" << endl;
      setupSupport.supportFullyConnectedNetworkCreation(nodeCount);
      cout << "> Done" << endl;

      cout << "> Publish RemoteAddress" << endl;
      setupSupport.supportRemoteMemoryAddressPublishing();
      cout << "> Done" << endl;

      //      cout << "[PRESS ENTER TO CONTINUE]" << endl;
      //   cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      //      cin.get();
   }
}
