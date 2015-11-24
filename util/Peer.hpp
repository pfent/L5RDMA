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
#include "MemoryRef.hpp"
//---------------------------------------------------------------------------
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <zmq.hpp>
#include <thread>
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
class MemoryRef;
class Hostname;
//---------------------------------------------------------------------------
class Peer {
   zmq::context_t &context;

   std::unique_ptr<zmq::socket_t> masterSocket;
   std::unique_ptr<zmq::socket_t> broadcastSocket;

   std::unique_ptr<zmq::socket_t> barrierMasterSocket;
   std::unique_ptr<zmq::socket_t> barrierBroadcastSocket;

   std::unique_ptr<zmq::socket_t> peerServerSocket;
   std::vector<std::unique_ptr<zmq::socket_t>> peerClientSockets;

   uint32_t nodeCount;
   std::string coordinatorHostname;
   bool verbose;

   uint32_t localId;
   std::vector<Hostname> peerHostnames;

   std::unique_ptr<std::thread> thread;
   std::mutex muhhh;
   std::unordered_map<std::string, MemoryRef> publishedData;

   std::atomic<bool> running;

public:
   Peer(zmq::context_t &context, uint32_t nodeCount, const std::string &coordinatorHostName);
   ~Peer();

   void exchangeHostnames();
   void startPublisherService();

   void publish(const std::string &key, const MemoryRef &memory);
   std::vector<uint8_t> lookUp(const std::string &key, uint32_t hostId);

   // requires a coordinator on [HOSTNAME] running "supportBarrier"
   void barrier();

   uint32_t getLocalId() { return localId; }
   int getNodeCount() { return nodeCount; }
};
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
