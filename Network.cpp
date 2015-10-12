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
#include "Network.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <infiniband/verbs.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include <unistd.h>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
MemoryRegion::MemoryRegion(void *address, size_t size, ibv_pd *protectionDomain) : address(address), size(size) {
   key = ::ibv_reg_mr(protectionDomain, address, size, IBV_ACCESS_LOCAL_WRITE);
   if (key == nullptr) {
      string reason = "registering memory failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
MemoryRegion::~MemoryRegion() {
   if (::ibv_dereg_mr(key) != 0) {
      string reason = "deregistering memory failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
ibv_qp *Network::createQueuePair(ibv_cq *cqSend, ibv_cq *cqRecv)
   /// Create queue pair
{
   ibv_qp_init_attr queuePairAttributes;
   memset(&queuePairAttributes, 0, sizeof(queuePairAttributes));
   queuePairAttributes.qp_context = nullptr;          // Associated context of the QP
   queuePairAttributes.send_cq = cqSend;              // CQ to be associated with the Send Queue (SQ)
   queuePairAttributes.recv_cq = cqRecv;              // CQ to be associated with the Receive Queue (RQ)
   queuePairAttributes.srq = srq;                     // SRQ handle if QP is to be associated with an SRQ, otherwise NULL
   queuePairAttributes.cap.max_send_wr = 16351;       // Requested max number of outstanding WRs in the SQ
   queuePairAttributes.cap.max_recv_wr = 16351;       // Requested max number of outstanding WRs in the RQ
   queuePairAttributes.cap.max_send_sge = 1;          // Requested max number of scatter/gather elements in a WR in the SQ
   queuePairAttributes.cap.max_recv_sge = 1;          // Requested max number of scatter/gather elements in a WR in the RQ
   queuePairAttributes.cap.max_inline_data = 512;     // Requested max number of bytes that can be posted inline to the SQ, otherwise 0
   queuePairAttributes.qp_type = IBV_QPT_RC;          // QP Transport Service Type: IBV_QPT_RC (reliable connection), IBV_QPT_UC (unreliable connection), or IBV_QPT_UD (unreliable datagram)
   queuePairAttributes.sq_sig_all = 0;                // If set, each Work Request (WR) submitted to the SQ generates a completion entry

   // Create queue pair
   ibv_qp *queuePair = ::ibv_create_qp(protectionDomain, &queuePairAttributes);
   if (!queuePair) {
      string reason = "creating the queue pair failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   return queuePair;
}
//---------------------------------------------------------------------------
string stringForCompletionCode(int opcode)
   /// Lookup the type of the completion event
{
   string description;
   switch (opcode) {
      case IBV_WC_RECV:
      description = "IBV_WC_RECV";
      break;
      case IBV_WC_SEND:
      description = "IBV_WC_SEND";
      break;
      case IBV_WC_RDMA_WRITE:
      description = "IBV_WC_RDMA_WRITE";
      break;
      case IBV_WC_RDMA_READ:
      description = "IBV_WC_RDMA_READ";
      break;
      case IBV_WC_COMP_SWAP:
      description = "IBV_WC_COMP_SWAP";
      break;
      case IBV_WC_FETCH_ADD:
      description = "IBV_WC_FETCH_ADD";
      break;
      case IBV_WC_BIND_MW:
      description = "IBV_WC_BIND_MW";
      break;
      case IBV_WC_RECV_RDMA_WITH_IMM:
      description = "IBV_WC_RECV_RDMA_WITH_IMM";
      break;
   }
   return description;
}
//---------------------------------------------------------------------------
uint64_t Network::pollCompletionQueue(ibv_cq *completionQueue, int type)
   /// Poll a completion queue
{
   int status;

   // Poll for a work completion
   ibv_wc completion;
   status = ::ibv_poll_cq(completionQueue, 1, &completion);
   if (status == 0) {
      return numeric_limits<uint64_t>::max();
   }

   // Check status and opcode
   if (completion.status == IBV_WC_SUCCESS) {
      if (completion.opcode == type) {
         return completion.wr_id;
      } else {
         string reason = "unexpected completion opcode (" + stringForCompletionCode(completion.opcode) + ")";
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   } else {
      string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
pair<bool, uint64_t> Network::waitForCompletion(bool restrict, bool onlySend)
   /// Wait for a work completion
{
   unique_lock<mutex> lock(completionMutex);
   int status;

   // We have to empty the completion queue and cache additional completions
   // as events are only generated when new work completions are enqueued.

   pair<bool, uint64_t> workCompletion;
   bool found = false;

   for (unsigned c = 0; c != cachedCompletions.size(); ++c) {
      if (!restrict || cachedCompletions[c].first == onlySend) {
         workCompletion = cachedCompletions[c];
         cachedCompletions.erase(cachedCompletions.begin() + c);
         found = true;
         break;
      }
   }

   while (!found) {
      // Wait for completion queue event
      ibv_cq* event;
      void* ctx;
      status = ::ibv_get_cq_event(completionChannel, &event, &ctx);
      if (status != 0) {
         string reason = "receiving the completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
         cerr << reason << endl;
         throw NetworkException(reason);
      }
      ::ibv_ack_cq_events(event, 1);
      bool isSendCompletion = (event == completionQueueSend);

      // Request a completion queue event
      status = ::ibv_req_notify_cq(event, 0);
      if (status != 0) {
         string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
         cerr << reason << endl;
         throw NetworkException(reason);
      }

      // Poll all work completions
      ibv_wc completion;
      do {
         status = ::ibv_poll_cq(event, 1, &completion);

         if (status < 0) {
            string reason = "failed to poll completions";
            cerr << reason << endl;
            throw NetworkException(reason);
         }
         if (status == 0) {
            continue;
         }

         if (completion.status != IBV_WC_SUCCESS) {
            string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
            cerr << reason << endl;
            throw NetworkException(reason);
         }

         // Add completion
         if (!found && (!restrict || isSendCompletion == onlySend)) {
            workCompletion = make_pair(isSendCompletion, completion.wr_id);
            found = true;
         } else {
            cachedCompletions.push_back(make_pair(isSendCompletion, completion.wr_id));
         }
      } while (status);
   }

   // Return the oldest completion
   return workCompletion;
}
//---------------------------------------------------------------------------
Network::Network(unsigned queuePairCount) : queuePairCount(queuePairCount), ibport(1)
   /// Constructor
{
   // Get the device list
   int deviceCount;
   devices = ::ibv_get_device_list(&deviceCount);
   if (!devices) {
      string reason = "unable to get the list of available devices";
      cerr << reason << endl;
      throw NetworkException(reason);
   } else if (deviceCount == 0) {
      string reason = "no Infiniband devices available";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Get the verbs context
   context = ::ibv_open_device(devices[0]);
   if (!context) {
      string reason = "unable to open the device";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create the protection domain
   protectionDomain = ::ibv_alloc_pd(context);
   if (protectionDomain == nullptr) {
      string reason = "allocating the protection domain failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create event channel
   completionChannel = ::ibv_create_comp_channel(context);
   if (completionChannel == nullptr) {
      string reason = "creating the shared completion channel failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create completion queues
   completionQueueSend = ::ibv_create_cq(context, CQ_SIZE, nullptr, completionChannel, 0);
   if (completionQueueSend == nullptr) {
      string reason = "creating the send completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   completionQueueRecv = ::ibv_create_cq(context, CQ_SIZE, nullptr, completionChannel, 0);
   if (completionQueueRecv == nullptr) {
      string reason = "creating the receive completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Request notifications
   int status = ::ibv_req_notify_cq(completionQueueSend, 0);
   if (status != 0) {
      string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   status = ::ibv_req_notify_cq(completionQueueRecv, 0);
   if (status != 0) {
      string reason = "requesting a completion queue event failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create shared receive queue
   struct ibv_srq_init_attr srq_init_attr;
   memset(&srq_init_attr, 0, sizeof(srq_init_attr));
   srq_init_attr.attr.max_wr  = 16351;
   srq_init_attr.attr.max_sge = 1;
   srq = ibv_create_srq(protectionDomain, &srq_init_attr);
   if (!srq) {
      string reason = "could not create shared receive queue";
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Create queue pairs
   for (unsigned i = 0; i != queuePairCount; ++i) {
      queuePairs.push_back(createQueuePair(completionQueueSend, completionQueueRecv));
   }
}
//---------------------------------------------------------------------------
Network::~Network()
   /// Destructor
{
   int status;

   // Destroy queue pairs
   for (unsigned i = 0; i != queuePairCount; ++i) {
      status = ::ibv_destroy_qp(queuePairs[i]);
      if (status != 0) {
         string reason = "destroying the queue pair failed with error " + to_string(errno) + ": " + strerror(errno);
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   }
   queuePairs.clear();

   // Destroy the shared receive queue
   status = ::ibv_destroy_srq(srq);
   if (status != 0) {
      string reason = "destroying the shared receive queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Destroy the completion queues
   status = ::ibv_destroy_cq(completionQueueSend);
   if (status != 0) {
      string reason = "destroying the send completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   status = ::ibv_destroy_cq(completionQueueRecv);
   if (status != 0) {
      string reason = "destroying the receive completion queue failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Destroy the completion channel
   status = ::ibv_destroy_comp_channel(completionChannel);
   if (status != 0) {
      string reason = "destroying the shared completion channel failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Deallocate the protection domain
   status = ::ibv_dealloc_pd(protectionDomain);
   if (status != 0) {
      string reason = "deallocating the protection domain failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Close context
   status = ::ibv_close_device(context);
   if (status != 0) {
      string reason = "closing the verbs context failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }

   // Free devices
   ::ibv_free_device_list(devices);
}
//---------------------------------------------------------------------------
uint16_t Network::getLID()
   /// Get the LID
{
   struct ibv_port_attr attributes;
   int status = ::ibv_query_port(context, ibport, &attributes);
   if (status != 0) {
      string reason = "querying port " + to_string(ibport) + " failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
   return attributes.lid;
}
//---------------------------------------------------------------------------
uint32_t Network::getQPN(unsigned index)
   /// Get the queue pair number for a queue pair
{
   return queuePairs[index]->qp_num;
}
//---------------------------------------------------------------------------
void Network::connect(vector<Address> addresses, unsigned retryCount)
   /// Connect the network
{
   uint32_t remotePSN = 0;
   uint32_t localPSN = 0;
   for (unsigned i = 0; i != queuePairCount; ++i) {
      struct ibv_qp_attr attributes;

      // INIT
      memset(&attributes, 0, sizeof(attributes));
      attributes.qp_state = IBV_QPS_INIT;
      attributes.pkey_index = 0;       // Partition the queue pair belongs to
      attributes.port_num = ibport;    // The local physical port
      attributes.qp_access_flags = 0;  // Allowed access flags of the remote operations for incoming packets (i.e., none, RDMA read, RDMA write, or atomics)
      if (::ibv_modify_qp(queuePairs[i], &attributes, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
         string reason = "failed to transition QP to INIT state";
         cerr << reason << endl;
         throw NetworkException(reason);
      }

      // RTR (ready to receive)
      memset(&attributes, 0, sizeof(attributes));
      attributes.qp_state = IBV_QPS_RTR;
      attributes.path_mtu = IBV_MTU_4096;             // Maximum payload size
      attributes.dest_qp_num = addresses[i].qpn;      // The remote QP number
      attributes.rq_psn = remotePSN;                  // The packet sequence number of received packets
      attributes.max_dest_rd_atomic = 16;             // The number of outstanding RDMA reads & atomic operations (destination)
      attributes.min_rnr_timer = 12;                  // The time before a RNR NACK is sent
      attributes.ah_attr.is_global = 0;               // Whether there is a global routing header
      attributes.ah_attr.dlid = addresses[i].lid;     // The LID of the remote host
      attributes.ah_attr.sl = 0;                      // The service level (which determines the virtual lane)
      attributes.ah_attr.src_path_bits = 0;           // Use the port base LID
      attributes.ah_attr.port_num = ibport;           // The local physical port
      if (::ibv_modify_qp(queuePairs[i], &attributes, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER)) {
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
      if (::ibv_modify_qp(queuePairs[i], &attributes, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC)) {
         string reason = "failed to transition QP to RTS state";
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   }
}
//---------------------------------------------------------------------------
void Network::postSend(unsigned target, const MemoryRegion& mr, bool completion, uint64_t context, int flags)
   /// Send a work request
{
   // Add the memory region to the scatter/gather list
   ibv_sge sge;
   sge.addr = reinterpret_cast<uintptr_t>(mr.address);   // Start address of the local memory buffer
   sge.length = mr.size;                                 // Length of the buffer
   if (!(flags & IBV_SEND_INLINE)) {
      sge.lkey = mr.key->lkey;                           // Key of the local Memory Region
   }

   // Create the work request
   ibv_send_wr workRequest;
   memset(&workRequest, 0, sizeof(workRequest));
   workRequest.wr_id = context;                                   // User defined WR ID
   workRequest.next = nullptr;                                    // Pointer to next WR in list, NULL if last WR
   workRequest.sg_list = &sge;                                    // Pointer to the s/g array
   workRequest.num_sge = 1;                                       // Size of the s/g array
   workRequest.opcode = IBV_WR_SEND;                              // Operation type
   workRequest.send_flags = flags | (completion ? IBV_SEND_SIGNALED : 0); // Request completion notification

   // Post work request
   ibv_send_wr *badWorkRequest = nullptr;
   int status = ::ibv_post_send(queuePairs[target], &workRequest, &badWorkRequest);
   if (status != 0) {
      string reason = "posting the work request failed with error " + to_string(status) + ": " + strerror(status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
void Network::postRecv(const MemoryRegion& mr, uint64_t context)
   /// Post a receive request
{
   // Add the memory region to the scatter/gather list
   ibv_sge sge;
   sge.addr = reinterpret_cast<uintptr_t>(mr.address);  // Start address of the local memory buffer
   sge.length = mr.size;                                // Length of the buffer
   sge.lkey = mr.key->lkey;                             // Key of the local Memory Region

   // Create the receive request
   ibv_recv_wr workRequest;
   memset(&workRequest, 0, sizeof(workRequest));
   workRequest.wr_id = context;                         // User defined WR ID
   workRequest.next = nullptr;                          // Pointer to next WR in list, NULL if last WR
   workRequest.sg_list = &sge;                          // Pointer to the s/g array
   workRequest.num_sge = 1;                             // Size of the s/g array

   // Post work request
   ibv_recv_wr *badWorkRequest = nullptr;
   int status = ::ibv_post_srq_recv(srq, &workRequest, &badWorkRequest);
   if (status != 0) {
      string reason = "posting the receive work request failed with error " + to_string(errno) + ": " + strerror(errno);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
uint64_t Network::pollSendCompletionQueue()
   /// Poll the send completion queue
{
   return pollCompletionQueue(completionQueueSend, IBV_WC_SEND);
}
//---------------------------------------------------------------------------
uint64_t Network::pollRecvCompletionQueue()
   /// Poll the receive completion queue
{
   return pollCompletionQueue(completionQueueRecv, IBV_WC_RECV);
}
//---------------------------------------------------------------------------
uint64_t Network::pollCompletionQueueBlocking(ibv_cq *completionQueue, int type)
   /// Poll a completion queue blocking
{
   int status;

   // Poll for a work completion
   ibv_wc completion;
   status = ::ibv_poll_cq(completionQueue, 1, &completion);
   while (status == 0) {
      status = ::ibv_poll_cq(completionQueue, 1, &completion);
   }

   // Check status and opcode
   if (completion.status == IBV_WC_SUCCESS) {
      if (completion.opcode == type) {
         return completion.wr_id;
      } else {
         string reason = "unexpected completion opcode (" + stringForCompletionCode(completion.opcode) + ")";
         cerr << reason << endl;
         throw NetworkException(reason);
      }
   } else {
      string reason = "unexpected completion status " + to_string(completion.status) + ": " + ibv_wc_status_str(completion.status);
      cerr << reason << endl;
      throw NetworkException(reason);
   }
}
//---------------------------------------------------------------------------
uint64_t Network::pollSendCompletionQueueBlocking()
   /// Poll the send completion queue blocking
{
   return pollCompletionQueueBlocking(completionQueueSend, IBV_WC_SEND);
}
//---------------------------------------------------------------------------
uint64_t Network::pollRecvCompletionQueueBlocking()
   /// Poll the receive completion queue blocking
{
   return pollCompletionQueueBlocking(completionQueueRecv, IBV_WC_RECV);
}
//---------------------------------------------------------------------------
pair<bool, uint64_t> Network::waitForCompletion()
   /// Wait for a work completion
{
   return waitForCompletion(false, false);
}
//---------------------------------------------------------------------------
uint64_t Network::waitForCompletionSend()
   /// Wait for a work completion
{
   return waitForCompletion(true, true).second;
}
//---------------------------------------------------------------------------
uint64_t Network::waitForCompletionReceive()
   /// Wait for a work completion
{
   return waitForCompletion(true, false).second;
}
//---------------------------------------------------------------------------
void Network::printCapabilities()
   /// Print the capabilities of the RDMA host channel adapter
{
	// Get a list of all devices
	int num_devices;
	struct ibv_device **device_list = ibv_get_device_list(&num_devices);
	if (!device_list) {
		std::cerr << "Error, querying the list of InfiniBand devices failed";
	} else {
		std::cout << num_devices << " InfiniBand device/s found";
	}

	// Open the first device
	struct ibv_context *context = ibv_open_device(device_list[0]);
	if (!context) {
		std::cerr << "Error, opening device " << ibv_get_device_name(device_list[0]) << " failed";
	} else {
		std::cout << "Opened the device " << ibv_get_device_name(context->device);
	}

	// Query device attributes
	struct ibv_device_attr device_attr;
	int status = ibv_query_device(context, &device_attr);
	if (status) {
		std::cerr << "Error, quering the attributes of device " << ibv_get_device_name(context->device) << " failed";
	}

	// Print attributes
	std::cout << "[Device Information]" << std::endl;
	std::cout << std::left << std::setw(44) << "  Device Name: " << ibv_get_device_name(context->device) << std::endl;
	std::cout << std::left << std::setw(44) << "  GUID: " << ibv_get_device_guid(context->device) << std::endl;
	std::cout << std::left << std::setw(44) << "  Vendor ID: " << device_attr.vendor_id << std::endl;
	std::cout << std::left << std::setw(44) << "  Vendor Part ID: " << device_attr.vendor_part_id << std::endl;
	std::cout << std::left << std::setw(44) << "  Hardware Version: " << device_attr.hw_ver << std::endl;
	std::cout << std::left << std::setw(44) << "  Firmware Version: " << device_attr.fw_ver << std::endl;
	std::cout << std::left << std::setw(44) << "  Physical Ports: " << device_attr.phys_port_cnt << std::endl;
	std::cout << std::left << std::setw(44) << "  CA ACK Delay: " << device_attr.local_ca_ack_delay << std::endl;

	std::cout << "[Memory]" << std::endl;
	std::cout << std::left << std::setw(44) << "  Max MR size: " << device_attr.max_mr_size << std::endl;
	std::cout << std::left << std::setw(44) << "  Max page size: " << device_attr.page_size_cap << std::endl;

	std::cout << "[Capabilities]" << std::endl;
	if (device_attr.device_cap_flags & IBV_DEVICE_RESIZE_MAX_WR) {
		std::cout << "  The device supports modifying the maximum number of outstanding Work Requests of a QP" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_BAD_PKEY_CNTR) {
		std::cout << "  The device supports bad P_Key counting for each port" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_BAD_QKEY_CNTR) {
		std::cout << "  The device supports P_Key violations counting for each port" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_RAW_MULTI) {
		std::cout << "  The device supports raw packet multicast" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_AUTO_PATH_MIG) {
		std::cout << "  The device supports automatic path migration" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_CHANGE_PHY_PORT) {
		std::cout << "  The device supports changing the primary port number of a QP when transitioning from SQD to SQD state" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_UD_AV_PORT_ENFORCE) {
		std::cout << "  The device supports AH port number enforcement" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_CURR_QP_STATE_MOD) {
		std::cout << "  The device supports the Current QP state modifier when calling ibv_modify_qp()" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_SHUTDOWN_PORT) {
		std::cout << "  The device supports shutdown port" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_INIT_TYPE) {
		std::cout << "  The device supports setting InitType and InitTypeReply" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_PORT_ACTIVE_EVENT) {
		std::cout << "  The device supports the IBV_EVENT_PORT_ACTIVE event generation" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_SYS_IMAGE_GUID) {
		std::cout << "  The device supports System Image GUID" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_RC_RNR_NAK_GEN) {
		std::cout << "  The device supports RNR-NAK generation for RC QPs" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_SRQ_RESIZE) {
		std::cout << "  The device supports modifying the maximum number of outstanding Work Requests in an SRQ" << std::endl;
	}
	if (device_attr.device_cap_flags & IBV_DEVICE_N_NOTIFY_CQ) {
		std::cout << "  The device supports Requesting Completion notification when N completions were added (and not only one) to a CQ" << std::endl;
	}

	std::cout << "[Resources]" << std::endl;
	std::cout << std::setw(44) << "  Max number of QPs: " << device_attr.max_qp << std::endl;
	std::cout << std::setw(44) << "  Max number of WRs per Queue: " << device_attr.max_qp_wr << std::endl;
	std::cout << std::setw(44) << "  Max number of SGE per WR: " << device_attr.max_sge << std::endl;
	std::cout << std::setw(44) << "  Max number of CQs: " << device_attr.max_cq << std::endl;
	std::cout << std::setw(44) << "  Max number of CQEs per CQ: " << device_attr.max_cqe << std::endl;
	std::cout << std::setw(44) << "  Max number of PDs: " << device_attr.max_pd << std::endl;
	std::cout << std::setw(44) << "  Max number of MRs: " << device_attr.max_mr << std::endl;
	std::cout << std::setw(44) << "  Max number of AHs: " << device_attr.max_ah << std::endl;
	std::cout << std::setw(44) << "  Max number of partitions: " << device_attr.max_pkeys << std::endl;

	std::cout << "[Multicast]" << std::endl;
	std::cout << std::setw(44) << "  Max multicast groups: " << device_attr.max_mcast_grp << std::endl;
	std::cout << std::setw(44) << "  Max QPs per multicast group: " << device_attr.max_mcast_qp_attach << std::endl;
	std::cout << std::setw(44) << "  Max total multicast QPs: " << device_attr.max_total_mcast_qp_attach << std::endl;

	std::cout << "[Atomics]" << std::endl;
	switch (device_attr.atomic_cap) {
		case(IBV_ATOMIC_NONE):
		std::cout << "  Atomic operations aren’t supported at all" << std::endl;
		break;
		case(IBV_ATOMIC_HCA):
		std::cout << "  Atomicity is guaranteed between QPs on this device only" << std::endl;
		break;
		case(IBV_ATOMIC_GLOB):
		std::cout << "  Atomicity is guaranteed between this device and any other component, such as CPUs and other devices" << std::endl;
		break;
	}
	std::cout << std::setw(44) << "  Max outstanding reads/atomics per QP: " <<  device_attr.max_qp_rd_atom << std::endl;
	std::cout << std::setw(44) << "  Resources for reads/atomics: " <<  device_attr.max_res_rd_atom << std::endl;
	std::cout << std::setw(44) << "  Max depth per QP read/atomic initiation: " <<  device_attr.max_qp_init_rd_atom << std::endl;

	std::cout << "[Reliable Datagram]" << std::endl;
	std::cout << std::setw(44) << "  Max number of SGEs per QP: " << device_attr.max_sge_rd << std::endl;
	std::cout << std::setw(44) << "  Max number of EECs: " << device_attr.max_ee << std::endl;
	std::cout << std::setw(44) << "  Max number of RDDs: " << device_attr.max_rdd << std::endl;
	std::cout << std::setw(44) << "  Max outstanding reads/atomics per EEC: " <<  device_attr.max_ee_rd_atom << std::endl;
	std::cout << std::setw(44) << "  Max depth per EEC read/atomic initiation: " <<  device_attr.max_ee_init_rd_atom << std::endl;

	std::cout << "[Memory Windows]" << std::endl;
	std::cout << std::setw(44) << "  Max number of MWs: " << device_attr.max_mw << std::endl;

	std::cout << "[Fast Memory Registration]" << std::endl;
	std::cout << std::setw(44) << "  Max number of FMRs: " << device_attr.max_fmr << std::endl;
	std::cout << std::setw(44) << "  Max number of maps per FMR: " << device_attr.max_map_per_fmr << std::endl;

	std::cout << "[Shared Receive Queues]" << std::endl;
	std::cout << std::setw(44) << "  Max number of SRQs: " << device_attr.max_srq << std::endl;
	std::cout << std::setw(44) << "  Max number of WR per SRQ: " << device_attr.max_srq_wr << std::endl;
	std::cout << std::setw(44) << "  Max number of SGEs per WR: " << device_attr.max_srq_sge << std::endl;

	std::cout << "[Raw]" << std::endl;
	std::cout << std::setw(44) << "  Max number of IPv6 QPs: " << device_attr.max_raw_ipv6_qp << std::endl;
	std::cout << std::setw(44) << "  Max number of Ethertype QPs: " << device_attr.max_raw_ethy_qp << std::endl;

	// Close the device
	status = ibv_close_device(context);
	if (status) {
		std::cerr << "Error, closing the device " << ibv_get_device_name(context->device) << " failed";
	}
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
