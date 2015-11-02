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
/// Info about each client in the network.
/// Some ordering as the qp in the network class.
struct PeerInfo {
   static const int MAX_HOSTNAME_SIZE = 32;

   PeerInfo(const rdma::Address &address, const std::string &hostname);

   rdma::Address address;
   char hostname[MAX_HOSTNAME_SIZE]; // Needed to connect via tcp to request more info ..
};
std::ostream &operator<<(std::ostream &os, const PeerInfo &peerInfo);
//---------------------------------------------------------------------------
/// This guy has all the code for the clients to setup a fully connected rdma network
struct TestHarness {
   zmq::context_t &context;

   std::unique_ptr <zmq::socket_t> masterSocket;
   std::unique_ptr <zmq::socket_t> broadcastSocket;

   rdma::Network network;
   std::vector <std::unique_ptr<rdma::QueuePair>> queuePairs;

   uint32_t localId;
   uint32_t nodeCount;
   std::string coordinatorHostName;
   bool verbose;

   std::vector <PeerInfo> peerInfos;

   TestHarness(zmq::context_t &context, uint32_t nodeCount, const std::string &coordinatorHostName);
   ~TestHarness();

   // requires a coordinator on [HOSTNAME] running "supportFullyConnectedNetworkCreation"
   void createFullyConnectedNetwork();

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   void publishAddress(rdma::RemoteMemoryRegion &remoteMemoryRegion);

   // requires a coordinator on [HOSTNAME] running "supportRemoteMemoryAddressPublishing"
   rdma::RemoteMemoryRegion retrieveAddress();
};
//---------------------------------------------------------------------------
/// This guy has all the code for a coordinator to setup a fully connected rdma network
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
