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
#include "Coordinator.hpp"
// ---------------------------------------------------------------------------
#include <iostream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unistd.h>
#include <thread>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
Hostname::Hostname(const string &hostname)
{
   assert(hostname.size()<Hostname::MAX_HOSTNAME_SIZE - 1);
   auto length = hostname.copy(this->hostname, hostname.size());
   this->hostname[length] = '\0';
}
//---------------------------------------------------------------------------
string Hostname::asString() const
{
   return string(hostname);
}
//---------------------------------------------------------------------------
ostream &operator<<(ostream &os, const Hostname &hostname)
{
   return os << "hostname='" << hostname.hostname << "'";
};
//---------------------------------------------------------------------------
Coordinator::Coordinator(zmq::context_t &context, uint32_t nodeCount, const string &coordinatorHostname)
        : context(context)
          , nodeCount(nodeCount)
          , coordinatorHostname(coordinatorHostname)
          , verbose(getenv("VERBOSE") != nullptr)
{
   // Normal socket pair
   {
      // Create sign-up socket
      masterSocket = make_unique<zmq::socket_t>(context, ZMQ_REP);
      masterSocket->bind("tcp://*:8028");

      // Create broadcast socket
      broadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_PUB);
      broadcastSocket->bind("tcp://*:8029");
   }

   // Barrier socket pair
   {
      // Create sign-up socket
      barrierMasterSocket = make_unique<zmq::socket_t>(context, ZMQ_REP);
      barrierMasterSocket->bind("tcp://*:8030");

      // Create broadcast socket
      barrierBroadcastSocket = make_unique<zmq::socket_t>(context, ZMQ_PUB);
      barrierBroadcastSocket->bind("tcp://*:8031");
   }
}
//---------------------------------------------------------------------------
Coordinator::~Coordinator()
{
   masterSocket->close();
   broadcastSocket->close();
   barrierMasterSocket->close();
   barrierBroadcastSocket->close();
}
//---------------------------------------------------------------------------
void Coordinator::supportHostnameExchange()
{
   // Connect to all nodes
   zmq::message_t allAddresses(nodeCount * sizeof(Hostname));
   Hostname *target = reinterpret_cast<Hostname *>(allAddresses.data());
   for (uint32_t i = 0; i<nodeCount; ++i) {
      zmq::message_t request;
      masterSocket->recv(&request);
      assert(request.size() == sizeof(Hostname));
      target[i] = *(Hostname *) request.data();

      zmq::message_t reply(sizeof(uint32_t));
      *reinterpret_cast<uint32_t *>(reply.data()) = i;
      masterSocket->send(reply);
   }

   // Broadcast to everyone
   broadcastSocket->send(allAddresses);
}
//---------------------------------------------------------------------------
void Coordinator::supportBarrier()
{
   new thread([=]() {
      while (true) {
         // Wait for a message from everyone
         for (uint32_t i = 0; i<nodeCount; ++i) {
            zmq::message_t msg(sizeof(uint64_t));
            barrierMasterSocket->recv(&msg);
            barrierMasterSocket->send(msg); // Does not matter
         }

         // Allow everyone to continue
         zmq::message_t msg(sizeof(uint64_t));
         barrierBroadcastSocket->send(msg);
      }
   });
}
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
