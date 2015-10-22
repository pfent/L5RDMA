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
namespace util {
//---------------------------------------------------------------------------
struct TestHarness {
   zmq::context_t &context;

   std::unique_ptr <zmq::socket_t> masterSocket;
   std::unique_ptr <zmq::socket_t> broadcastSocket;

   rdma::Network network;
   uint32_t localId;
   uint32_t nodeCount;
   std::string coordinatorHostName;
   bool verbose;

   TestHarness(zmq::context_t &context, uint32_t nodeCount, const std::string& coordinatorHostName);

   // requires a coordinator on [HOSTNAME] running "supportFullyConnectedNetworkCreation"
   void createFullyConnectedNetwork();

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   void publishAddress(rdma::RemoteMemoryRegion &remoteMemoryRegion);

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   rdma::RemoteMemoryRegion retrieveAddress();
};
//---------------------------------------------------------------------------
struct SetupSupport {
   zmq::context_t &context;

   std::unique_ptr <zmq::socket_t> masterSocket;
   std::unique_ptr <zmq::socket_t> broadcastSocket;

   SetupSupport(zmq::context_t &context);
   ~SetupSupport();

   void supportFullyConnectedNetworkCreation(uint32_t nodeCount);

   void supportRemoteMemoryAddressPublishing();
};
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
