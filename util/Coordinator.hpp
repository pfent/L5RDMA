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
#include <string>
#include <memory>
#include <zmq.hpp>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
//---------------------------------------------------------------------------
namespace rdma {
class QueuePair;
}
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
struct Hostname {
   static const int MAX_HOSTNAME_SIZE = 128;

   Hostname(const std::string &hostname);

   char hostname[MAX_HOSTNAME_SIZE];

   std::string asString() const;
};
std::ostream &operator<<(std::ostream &os, const Hostname &hostname);
//---------------------------------------------------------------------------
/// This guy has all the code for the clients to setup a fully connected rdma network
struct Coordinator {
   zmq::context_t &context;

   std::unique_ptr<zmq::socket_t> masterSocket;
   std::unique_ptr<zmq::socket_t> broadcastSocket;

   std::unique_ptr<zmq::socket_t> barrierMasterSocket;
   std::unique_ptr<zmq::socket_t> barrierBroadcastSocket;

   uint32_t nodeCount;
   std::string coordinatorHostname;
   bool verbose;

   std::vector<Hostname> hostnames;

   Coordinator(zmq::context_t &context, uint32_t nodeCount, const std::string &coordinatorHostName);
   ~Coordinator();

   void supportHostnameExchange();

   void supportBarrier();
};
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
