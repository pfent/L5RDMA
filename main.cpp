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
#include <iomanip>
#include <iostream>
#include <memory>
//---------------------------------------------------------------------------
#include "Network.hpp"
//---------------------------------------------------------------------------
using namespace std;
using namespace rdma;
//---------------------------------------------------------------------------
int MESSAGE_SIZE = 100;
//---------------------------------------------------------------------------
class RDMATest {
   int nodes;
   int id;
   Network network;

public:
   /// Constructor
   RDMATest(int nodes, int id) : nodes(nodes), id(id), network(nodes) {
      vector<Address> addresses(nodes);
      for (int node = 0; node != nodes; ++node) {
         if (node == id) {
            addresses[node] = {network.getLID(), network.getQPN(node)};
         } else {
            cout << "[" << node << "] " << network.getLID() << " " << network.getQPN(node) << endl;
            cin >> addresses[node].lid;
            cin >> addresses[node].qpn;
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
         }
      }
      network.connect(addresses);
   }

   void testSendReceive() {
      // receive message
      char *receive = new char[MESSAGE_SIZE];
      MemoryRegion receiveMR(receive, MESSAGE_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite);
      network.postRecv(receiveMR, 0);
      int ignore;
      cout << "[ENTER MESSAGE]" << endl;
      string message;
      getline(cin, message);

      // send message
      char *send = new char[MESSAGE_SIZE];
      snprintf(send, MESSAGE_SIZE, "%s", message.c_str());
      MemoryRegion sendMR(send, MESSAGE_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite | MemoryRegion::Permission::RemoteWrite);
      network.postSend((id+1)%nodes, sendMR, true, 0);
      network.waitForCompletionSend();
      network.waitForCompletionReceive();
      cout << "message: " << receive << endl;

      delete[] receive;
      delete[] send;
   }

   void testAtomics() {
   
   }
};
//---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
   if (argc != 3) {
      cerr << "Usage: nodes id" << endl;
      exit(EXIT_FAILURE);
   }
   int nodes = atoi(argv[1]);
   int id = atoi(argv[2]);
   
   RDMATest test(nodes, id);
   test.testAtomics();
}
