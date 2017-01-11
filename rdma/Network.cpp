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
#include "WorkRequest.hpp"
#include "QueuePair.hpp"
#include "ReceiveQueue.hpp"
#include "CompletionQueuePair.hpp"
//---------------------------------------------------------------------------
#include <cstring>
#include <infiniband/verbs.h>
#include <iostream>
#include <iomanip>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
ostream &operator<<(ostream &os, const RemoteMemoryRegion &remoteMemoryRegion)
{
   return os << "address=" << (void *) remoteMemoryRegion.address << " key=" << remoteMemoryRegion.key;
}
//---------------------------------------------------------------------------
ostream &operator<<(ostream &os, const Address &address)
{
   return os << "lid=" << address.lid << ", qpn=" << address.qpn;
}
//---------------------------------------------------------------------------
Network::Network()
        : ibport(1)
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
   } else if (deviceCount>1) {
      string reason = "more than 1 Infiniband devices available .. not handled right now";
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

   sharedReceiveQueue = make_unique<ReceiveQueue>(*this);
   sharedCompletionQueuePair = make_unique<CompletionQueuePair>(*this);
}
//---------------------------------------------------------------------------
Network::~Network()
/// Destructor
{
   int status;

   sharedReceiveQueue.release();
   sharedCompletionQueuePair.release();

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
      case (IBV_ATOMIC_NONE):
         std::cout << "  Atomic operations aren’t supported at all" << std::endl;
         break;
      case (IBV_ATOMIC_HCA):
         std::cout << "  Atomicity is guaranteed between QPs on this device only" << std::endl;
         break;
      case (IBV_ATOMIC_GLOB):
         std::cout << "  Atomicity is guaranteed between this device and any other component, such as CPUs and other devices" << std::endl;
         break;
   }
   std::cout << std::setw(44) << "  Max outstanding reads/atomics per QP: " << device_attr.max_qp_rd_atom << std::endl;
   std::cout << std::setw(44) << "  Resources for reads/atomics: " << device_attr.max_res_rd_atom << std::endl;
   std::cout << std::setw(44) << "  Max depth per QP read/atomic initiation: " << device_attr.max_qp_init_rd_atom << std::endl;

   std::cout << "[Reliable Datagram]" << std::endl;
   std::cout << std::setw(44) << "  Max number of SGEs per QP: " << device_attr.max_sge_rd << std::endl;
   std::cout << std::setw(44) << "  Max number of EECs: " << device_attr.max_ee << std::endl;
   std::cout << std::setw(44) << "  Max number of RDDs: " << device_attr.max_rdd << std::endl;
   std::cout << std::setw(44) << "  Max outstanding reads/atomics per EEC: " << device_attr.max_ee_rd_atom << std::endl;
   std::cout << std::setw(44) << "  Max depth per EEC read/atomic initiation: " << device_attr.max_ee_init_rd_atom << std::endl;

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
    RemoteMemoryRegion RemoteMemoryRegion::slice(size_t offset) {
        return RemoteMemoryRegion(this->address + offset, this->key);
    }
}
//---------------------------------------------------------------------------
