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
#include "QueuePair.hpp"
#include "WorkRequest.hpp"
#include "MemoryRegion.hpp"
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
QueuePair::QueuePair(ibv_qp *qp, Network &network)
        : qp(qp)
          , network(network)
{
}
//---------------------------------------------------------------------------
QueuePair::~QueuePair()
{
   int status = ::ibv_destroy_qp(qp);
   if (status != 0) {
      string reason = "destroying the queue pair failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // TODO: free ?
}
//---------------------------------------------------------------------------
uint32_t QueuePair::getQPN()
{
   return qp->qp_num;
}
//---------------------------------------------------------------------------
void QueuePair::connect(const Address &address, unsigned retryCount)
{
   network.connectQueuePair(*this, address, retryCount);
}
// -------------------------------------------------------------------------
void QueuePair::postWorkRequest(const WorkRequest &workRequest)
{
   ibv_send_wr *badWorkRequest = nullptr;
   int status = ::ibv_post_send(qp, workRequest.wr.get(), &badWorkRequest);
   if (status != 0) {
      string reason = "posting the work request failed with error " + to_string(status) + ": " + strerror(status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
// -------------------------------------------------------------------------
namespace { // Anonymous helper namespace
// -------------------------------------------------------------------------
string queuePairStateToString(ibv_qp_state qp_state)
{
   switch (qp_state) {
      case IBV_QPS_RESET:
         return "IBV_QPS_RESET";
      case IBV_QPS_INIT:
         return "IBV_QPS_INIT";
      case IBV_QPS_RTR:
         return "IBV_QPS_RTR";
      case IBV_QPS_RTS:
         return "IBV_QPS_RTS";
      case IBV_QPS_SQD:
         return "IBV_QPS_SQD";
      case IBV_QPS_SQE:
         return "IBV_QPS_SQE";
      case IBV_QPS_ERR:
         return "IBV_QPS_ERR";
      default:
         throw;
   }
}
// -------------------------------------------------------------------------
string queuePairAccessFlagsToString(int qp_access_flags)
{
   string result = "";
   if (qp_access_flags & IBV_ACCESS_REMOTE_WRITE)
      result += "IBV_ACCESS_REMOTE_WRITE, ";
   if (qp_access_flags & IBV_ACCESS_REMOTE_READ)
      result += "IBV_ACCESS_REMOTE_READ, ";
   if (qp_access_flags & IBV_ACCESS_REMOTE_ATOMIC)
      result += "IBV_ACCESS_REMOTE_ATOMIC, ";
   return result;
}
// -------------------------------------------------------------------------
} // end of anonymous helper namespace
// -------------------------------------------------------------------------
void QueuePair::printQueuePairDetails()
{
   struct ibv_qp_attr attr;
   struct ibv_qp_init_attr init_attr;
   memset(&attr, 0, sizeof(attr));
   memset(&init_attr, 0, sizeof(init_attr));

   const int allFlags = IBV_QP_STATE | IBV_QP_CUR_STATE | IBV_QP_EN_SQD_ASYNC_NOTIFY | IBV_QP_ACCESS_FLAGS | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_RQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_ALT_PATH | IBV_QP_MIN_RNR_TIMER | IBV_QP_SQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_PATH_MIG_STATE | IBV_QP_CAP | IBV_QP_DEST_QPN;
   if (::ibv_query_qp(qp, &attr, allFlags, &init_attr)) {
      string reason = "Error, querying the queue pair details.";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   cout << "[State of QP " << qp << "]" << endl;
   cout << endl;
   cout << left << setw(44) << "qp_state:" << queuePairStateToString(attr.qp_state) << endl;
   cout << left << setw(44) << "cur_qp_state:" << queuePairStateToString(attr.cur_qp_state) << endl;
   cout << left << setw(44) << "path_mtu:" << attr.path_mtu << endl;
   cout << left << setw(44) << "path_mig_state:" << attr.path_mig_state << endl;
   cout << left << setw(44) << "qkey:" << attr.qkey << endl;
   cout << left << setw(44) << "rq_psn:" << attr.rq_psn << endl;
   cout << left << setw(44) << "sq_psn:" << attr.sq_psn << endl;
   cout << left << setw(44) << "dest_qp_num:" << attr.dest_qp_num << endl;
   cout << left << setw(44) << "qp_access_flags:" << queuePairAccessFlagsToString(attr.qp_access_flags) << endl;
   cout << left << setw(44) << "cap:" << "<not impl>" << endl;
   cout << left << setw(44) << "ah_attr:" << "<not impl>" << endl;
   cout << left << setw(44) << "alt_ah_attr:" << "<not impl>" << endl;
   cout << left << setw(44) << "pkey_index:" << attr.pkey_index << endl;
   cout << left << setw(44) << "alt_pkey_index:" << attr.alt_pkey_index << endl;
   cout << left << setw(44) << "en_sqd_async_notify:" << static_cast<int>(attr.en_sqd_async_notify) << endl;
   cout << left << setw(44) << "sq_draining:" << static_cast<int>(attr.sq_draining) << endl;
   cout << left << setw(44) << "max_rd_atomic:" << static_cast<int>(attr.max_rd_atomic) << endl;
   cout << left << setw(44) << "max_dest_rd_atomic:" << static_cast<int>(attr.max_dest_rd_atomic) << endl;
   cout << left << setw(44) << "min_rnr_timer:" << static_cast<int>(attr.min_rnr_timer) << endl;
   cout << left << setw(44) << "port_num:" << static_cast<int>(attr.port_num) << endl;
   cout << left << setw(44) << "timeout:" << static_cast<int>(attr.timeout) << endl;
   cout << left << setw(44) << "retry_cnt:" << static_cast<int>(attr.retry_cnt) << endl;
   cout << left << setw(44) << "rnr_retry:" << static_cast<int>(attr.rnr_retry) << endl;
   cout << left << setw(44) << "alt_port_num:" << static_cast<int>(attr.alt_port_num) << endl;
   cout << left << setw(44) << "alt_timeout:" << static_cast<int>(attr.alt_timeout) << endl;
}
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
