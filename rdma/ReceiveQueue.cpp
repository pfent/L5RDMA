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
#include "ReceiveQueue.hpp"
#include "WorkRequest.hpp"
#include "Network.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <cstring>
#include <iostream>
#include <iomanip>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
ReceiveQueue::ReceiveQueue(Network &network)
{
   // Create receive queue
    struct ibv_srq_init_attr srq_init_attr{};
   memset(&srq_init_attr, 0, sizeof(srq_init_attr));
   srq_init_attr.attr.max_wr = 16351;
   srq_init_attr.attr.max_sge = 1;
   queue = ibv_create_srq(network.protectionDomain, &srq_init_attr);
   if (!queue) {
      string reason = "could not create receive queue";
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
ReceiveQueue::~ReceiveQueue()
{
   int status;

   // Destroy the receive queue
   status = ::ibv_destroy_srq(queue);
   if (status != 0) {
      string reason = "destroying the receive queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
