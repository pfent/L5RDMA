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
#include "Network.hpp"
#include "ReceiveQueue.hpp"
#include "CompletionQueuePair.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <cstring>
#include <iostream>
#include <iomanip>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {

    static const uint32_t maxInlineSize = 512;
//---------------------------------------------------------------------------
QueuePair::QueuePair(Network &network)
        : QueuePair(network, *network.sharedCompletionQueuePair, *network.sharedReceiveQueue)
{
}
//---------------------------------------------------------------------------
QueuePair::QueuePair(Network &network, ReceiveQueue &receiveQueue)
        : QueuePair(network, *network.sharedCompletionQueuePair, receiveQueue)
{
}
//---------------------------------------------------------------------------
QueuePair::QueuePair(Network &network, CompletionQueuePair &completionQueuePair)
        : QueuePair(network, completionQueuePair, *network.sharedReceiveQueue)
{
}
//---------------------------------------------------------------------------
QueuePair::QueuePair(Network &network, CompletionQueuePair &completionQueuePair, ReceiveQueue& receiveQueue)
        : network(network)
          , completionQueuePair(completionQueuePair)
{
   ibv_qp_init_attr queuePairAttributes;
   memset(&queuePairAttributes, 0, sizeof(queuePairAttributes));
   queuePairAttributes.qp_context = nullptr;                       // Associated context of the QP
   queuePairAttributes.send_cq = completionQueuePair.sendQueue;    // CQ to be associated with the Send Queue (SQ)
   queuePairAttributes.recv_cq = completionQueuePair.receiveQueue; // CQ to be associated with the Receive Queue (RQ)
   queuePairAttributes.srq = receiveQueue.queue;                   // SRQ handle if QP is to be associated with an SRQ, otherwise NULL
   queuePairAttributes.cap.max_send_wr = 16351;                    // Requested max number of outstanding WRs in the SQ
   queuePairAttributes.cap.max_recv_wr = 16351;                    // Requested max number of outstanding WRs in the RQ
   queuePairAttributes.cap.max_send_sge = 1;                       // Requested max number of scatter/gather elements in a WR in the SQ
   queuePairAttributes.cap.max_recv_sge = 1;                       // Requested max number of scatter/gather elements in a WR in the RQ
    queuePairAttributes.cap.max_inline_data = maxInlineSize;        // Requested max number of bytes that can be posted inline to the SQ, otherwise 0
   queuePairAttributes.qp_type = IBV_QPT_RC;                       // QP Transport Service Type: IBV_QPT_RC (reliable connection), IBV_QPT_UC (unreliable connection), or IBV_QPT_UD (unreliable datagram)
   queuePairAttributes.sq_sig_all = 0;                             // If set, each Work Request (WR) submitted to the SQ generates a completion entry

   // Create queue pair
   qp = ::ibv_create_qp(network.protectionDomain, &queuePairAttributes);
   if (!qp) {
      string reason = "creating the queue pair failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
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
   uint32_t remotePSN = 0;
   uint32_t localPSN = 0;

   struct ibv_qp_attr attributes{};

   // INIT
   memset(&attributes, 0, sizeof(attributes));
   attributes.qp_state = IBV_QPS_INIT;
   attributes.pkey_index = 0;               // Partition the queue pair belongs to
   attributes.port_num = network.ibport;    // The local physical port
   attributes.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC;  // Allowed access flags of the remote operations for incoming packets (i.e., none, RDMA read, RDMA write, or atomics)
   if (::ibv_modify_qp(qp, &attributes, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
      string reason = "failed to transition QP to INIT state";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // RTR (ready to receive)
   memset(&attributes, 0, sizeof(attributes));
   attributes.qp_state = IBV_QPS_RTR;
   attributes.path_mtu = IBV_MTU_4096;             // Maximum payload size
   attributes.dest_qp_num = address.qpn;           // The remote QP number
   attributes.rq_psn = remotePSN;                  // The packet sequence number of received packets
   attributes.max_dest_rd_atomic = 16;             // The number of outstanding RDMA reads & atomic operations (destination)
   attributes.min_rnr_timer = 12;                  // The time before a RNR NACK is sent
   attributes.ah_attr.is_global = 0;               // Whether there is a global routing header
   attributes.ah_attr.dlid = address.lid;          // The LID of the remote host
   attributes.ah_attr.sl = 0;                      // The service level (which determines the virtual lane)
   attributes.ah_attr.src_path_bits = 0;           // Use the port base LID
   attributes.ah_attr.port_num = network.ibport;   // The local physical port
   if (::ibv_modify_qp(qp, &attributes, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER)) {
      string reason = "failed to transition QP to RTR state";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // RTS (ready to send)
   memset(&attributes, 0, sizeof(attributes));
   attributes.qp_state = IBV_QPS_RTS;
   attributes.sq_psn = localPSN;       // The packet sequence number of sent packets
   attributes.timeout = 0;             // The minimum timeout before retransmitting the packet (0 = infinite)
   attributes.retry_cnt = retryCount;  // How often to retry sending (7 = infinite)
   attributes.rnr_retry = retryCount;  // How often to retry sending when RNR NACK was received (7 = infinite)
   attributes.max_rd_atomic = 128;     // The number of outstanding RDMA reads & atomic operations (initiator)
   if (::ibv_modify_qp(qp, &attributes, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC)) {
      string reason = "failed to transition QP to RTS state";
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
// -------------------------------------------------------------------------
void QueuePair::postWorkRequest(WorkRequest &workRequest)
{
   ibv_send_wr *badWorkRequest = nullptr;
   int status = ::ibv_post_send(qp, workRequest.get(), &badWorkRequest);
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
    void QueuePair::printQueuePairDetails() const
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

    uint32_t QueuePair::getMaxInlineSize() const {
        return maxInlineSize;
    }
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
