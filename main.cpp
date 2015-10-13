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
      vector <Address> addresses(nodes);
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
      cout << "----------> SendReceive" << endl;

      // receive message
      char *receive = new char[MESSAGE_SIZE];
      MemoryRegion receiveMR(receive, MESSAGE_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite);
      network.postRecv(receiveMR, 0);
      cout << "[ENTER MESSAGE]" << endl;
      string message;
      getline(cin, message);

      // send message
      char *send = new char[MESSAGE_SIZE];
      snprintf(send, MESSAGE_SIZE, "%s", message.c_str());
      MemoryRegion sendMR(send, MESSAGE_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite);
      int target = (id + 1) % nodes;
      network.postSend(target, sendMR, true, 0);
      network.waitForCompletionSend();
      network.waitForCompletionReceive();
      cout << "message: " << receive << endl;

      delete[] receive;
      delete[] send;
   }

   void testAtomicFetchAndAdd() {
      cout << "----------> AtomicFetchAndAdd" << endl;
      int target = (id + 1) % nodes;

      cout << "> pin before buffer" << endl;
      uint64_t *beforeValue = new uint64_t;
      *beforeValue = 8;
      MemoryRegion beforeValueMR(beforeValue, sizeof(*beforeValue), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> pin target buffer" << endl;
      uint64_t *targetValue = new uint64_t;
      *targetValue = 28;
      MemoryRegion targetMR(targetValue, sizeof(*targetValue), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> addrs & key: " << reinterpret_cast<uintptr_t>(targetMR.address) << " " << targetMR.key->rkey << endl;
      cout << "[ENTER REMOTE ADDRESS]" << endl;
      RemoteMemoryRegion remoteAddress;
      cin >> remoteAddress.address;
      cin >> remoteAddress.key;

      cout << "> sending to: " << remoteAddress.address << " " << remoteAddress.key << endl;
      network.postFetchAdd(target, beforeValueMR, remoteAddress, 42, true, 8028);
      network.waitForCompletionSend();

      cout << "[PRESS ENTER TO CONTINUE]" << endl;
      cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      cin.get();

      cout << "beforeValue: " << *beforeValue << endl; assert(*beforeValue == 28);
      cout << "targetValue: " << *targetValue << endl; assert(*targetValue == 70);

      delete beforeValue;
      delete targetValue;
   }

   // ORIGIN -> TARGET
   void testRemoteWrite() {
      cout << "----------> RemoteWrite" << endl;
      int target = (id + 1) % nodes;

      cout << "> pin origin buffer" << endl;
      vector <uint64_t> originBuffer(128);
      fill(originBuffer.begin(), originBuffer.end(), 28);
      MemoryRegion originMR(originBuffer.data(), sizeof(uint64_t) * originBuffer.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> pin target buffer" << endl;
      vector <uint64_t> targetBuffer(128);
      fill(targetBuffer.begin(), targetBuffer.end(), 8028);
      MemoryRegion targetMR(targetBuffer.data(), sizeof(uint64_t) * targetBuffer.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> addrs & key: " << reinterpret_cast<uintptr_t>(targetMR.address) << " " << targetMR.key->rkey << endl;
      cout << "[ENTER REMOTE ADDRESS]" << endl;
      RemoteMemoryRegion remoteAddress;
      cin >> remoteAddress.address;
      cin >> remoteAddress.key;

      cout << "> sending to: " << remoteAddress.address << " " << remoteAddress.key << endl;
      network.postWrite(target, remoteAddress, originMR, true, 8028);
      network.waitForCompletionSend();

      cout << "[PRESS ENTER TO CONTINUE]" << endl;
      cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      cin.get();

      for (uint i = 0; i<originBuffer.size(); i += 16) {
         cout << "i=" << i << ": " << originBuffer[i] << " " << targetBuffer[i] << endl;
         assert(originBuffer[i] == targetBuffer[i]);
      }
   }

   // TARGET <- ORIGIN
   void testRemoteRead() {
      cout << "----------> RemoteRead" << endl;
      int target = (id + 1) % nodes;

      cout << "> pin origin buffer" << endl;
      vector <uint64_t> originBuffer(128);
      fill(originBuffer.begin(), originBuffer.end(), 28);
      MemoryRegion originMR(originBuffer.data(), sizeof(uint64_t) * originBuffer.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> pin target buffer" << endl;
      vector <uint64_t> targetBuffer(128);
      fill(targetBuffer.begin(), targetBuffer.end(), 8028);
      MemoryRegion targetMR(targetBuffer.data(), sizeof(uint64_t) * targetBuffer.size(), network.getProtectionDomain(), MemoryRegion::Permission::All);

      cout << "> addrs & key: " << reinterpret_cast<uintptr_t>(originMR.address) << " " << originMR.key->rkey << endl;
      cout << "[ENTER REMOTE ADDRESS]" << endl;
      RemoteMemoryRegion remoteAddress;
      cin >> remoteAddress.address;
      cin >> remoteAddress.key;

      cout << "> sending to: " << remoteAddress.address << " " << remoteAddress.key << endl;
      network.postRead(target, targetMR, remoteAddress, true, 8028);
      network.waitForCompletionSend();

      cout << "[PRESS ENTER TO CONTINUE]" << endl;
      cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      cin.get();

      for (uint i = 0; i<originBuffer.size(); i += 16) {
         cout << "i=" << i << ": " << originBuffer[i] << " " << targetBuffer[i] << endl;
         assert(originBuffer[i] == targetBuffer[i]);
      }
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

//   test.testSendReceive();
//   test.testRemoteWrite();
//   test.testRemoteRead();
   test.testAtomicFetchAndAdd();
}
