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
#include "Peer.hpp"
#include "Coordinator.hpp"
#include "MemoryRef.hpp"
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
Peer::Peer(zmq::context_t &context, uint32_t nodeCount, const string &coordinatorHostname)
        : context(context)
          , nodeCount(nodeCount)
          , coordinatorHostname(coordinatorHostname)
          , verbose(getenv("VERBOSE") != nullptr)
          , localId(~1u)
{
   // Normal socket pair
   {
      // Request reply socket
      masterSocket = unique_ptr<zmq::socket_t>(new zmq::socket_t(context, ZMQ_REQ));
      masterSocket->connect(("tcp://" + coordinatorHostname + ":8028").c_str());

      // Broadcast socket
      broadcastSocket = unique_ptr<zmq::socket_t>(new zmq::socket_t(context, ZMQ_SUB));
      broadcastSocket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
      broadcastSocket->connect(("tcp://" + coordinatorHostname + ":8029").c_str());
   }

   // Barrier socket pair
   {
      // Barrier request reply socket
      barrierMasterSocket = unique_ptr<zmq::socket_t>(new zmq::socket_t(context, ZMQ_REQ));
      barrierMasterSocket->connect(("tcp://" + coordinatorHostname + ":8030").c_str());

      // Barrier broadcast socket
      barrierBroadcastSocket = unique_ptr<zmq::socket_t>(new zmq::socket_t(context, ZMQ_SUB));
      barrierBroadcastSocket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
      barrierBroadcastSocket->connect(("tcp://" + coordinatorHostname + ":8031").c_str());
   }

   // Server Socket
   {
      peerServerSocket = make_unique<zmq::socket_t>(context, ZMQ_REP);
      peerServerSocket->bind("tcp://*:8032");
   }

   // Wait for setup to finish .. great job zmq
   usleep(100000); // = 100ms
}
//---------------------------------------------------------------------------
Peer::~Peer()
{
   running = false;

   masterSocket->close();
   broadcastSocket->close();
   barrierMasterSocket->close();
   barrierBroadcastSocket->close();
   peerServerSocket->close();

   for (auto &clientSocket : peerClientSockets) {
      clientSocket->close();
   }

   if (thread != nullptr)
      thread->join();
}
//---------------------------------------------------------------------------
// requires a coordinator on [HOSTNAME] running "supportExchangeHostnames"
void Peer::exchangeHostnames()
{
   // Send the content of the created vector to the coordinator
   Hostname hostname(util::getHostname());
   zmq::message_t request(sizeof(Hostname));
   memcpy(request.data(), &hostname, sizeof(Hostname));
   masterSocket->send(request);
   if (verbose)
      cout << "> Sent hostname" << endl;

   // Receive Id
   zmq::message_t reply;
   masterSocket->recv(&reply);
   assert(reply.size() == sizeof(uint32_t));
   localId = *reinterpret_cast<uint32_t *>(reply.data());
   if (verbose)
      cout << "> Local id: " << localId << endl;

   // Receive hostNames of other clients
   zmq::message_t allHostnames(nodeCount * sizeof(Hostname));
   broadcastSocket->recv(&allHostnames);
   assert(allHostnames.size() == nodeCount * sizeof(Hostname));
   Hostname *hostNamePtr = reinterpret_cast<Hostname *>(allHostnames.data());
   for (uint32_t i = 0; i<nodeCount; i++) {
      // Connect to each other peer
      auto socket = unique_ptr<zmq::socket_t>(new zmq::socket_t(context, ZMQ_REQ));
      socket->connect(("tcp://" + hostNamePtr[i].asString() + ":8032").c_str());
      peerClientSockets.push_back(move(socket));
   }

   if (verbose)
      cout << ">> Done" << endl;
}
//---------------------------------------------------------------------------
void Peer::startPublisherService()
{
   running = true;
   thread = make_unique<::thread>([=]() {
      while (running) {
         zmq::pollitem_t items[] = {*peerServerSocket, 0, ZMQ_POLLIN, 0};
         int rc = zmq_poll(items, 1, 1000); // 1s

         if (rc<0) {
            cout << errno << endl;
            cout << zmq_errno() << endl;
            cout << zmq_strerror(zmq_errno()) << endl;
            throw;
         }

         if (items[0].revents != 0) {
            // Get request
            zmq::message_t request;
            peerServerSocket->recv(&request);
            string key((char *) request.data());

            // Lookup
            unique_lock<mutex> guard(muhhh);
            auto iter = publishedData.find(key);
            assert(iter != publishedData.end());
            MemoryRef result = iter->second;

            // Send back
            zmq::message_t reply(result.size());
            memcpy(reply.data(), result.data(), result.size()); // Need to keep lock at least till here (in case we implement remove)
            peerServerSocket->send(reply);
         }
      }
   });
}
//---------------------------------------------------------------------------
void Peer::publish(const std::string &key, const MemoryRef &memory)
{
   unique_lock<mutex> guardSays(muhhh);
   publishedData.insert(make_pair(key, memory));
}
//---------------------------------------------------------------------------
vector<uint8_t> Peer::lookUp(const std::string &key, uint32_t hostId)
{
   zmq::message_t request(key.size() + 1);
   memcpy(request.data(), key.c_str(), key.size() + 1);
   peerClientSockets[hostId]->send(request);

   zmq::message_t reply;
   peerClientSockets[hostId]->recv(&reply);
   vector<uint8_t> result(reply.size());
   memcpy(result.data(), reply.data(), reply.size());

   return result;
}
//---------------------------------------------------------------------------
// requires a coordinator on [HOSTNAME] running "supportBarrier"
void Peer::barrier()
{
   if (verbose) {
      cout << "> Entering Barrier" << endl;
   }

   zmq::message_t request(sizeof(uint32_t));
   barrierMasterSocket->send(request);
   barrierMasterSocket->recv(&request);
   barrierBroadcastSocket->recv(&request);

   if (verbose) {
      cout << "> Leaving Barrier" << endl;
   }
}
} // End of namespace util
//---------------------------------------------------------------------------
