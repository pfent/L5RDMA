#include "Network.hpp"
#include "CompletionQueuePair.hpp"
#include <cstring>
#include <iostream>
#include <iomanip>

using namespace std;
namespace rdma {
    ostream &operator<<(ostream &os, const RemoteMemoryRegion &remoteMemoryRegion) {
        return os << "address=" << reinterpret_cast<void *>(remoteMemoryRegion.address) << " key="
                  << remoteMemoryRegion.key;
    }

    ostream &operator<<(ostream &os, const Address &address) {
        return os << "lid=" << address.lid << ", qpn=" << address.qpn;
    }

    Network::Network() : devices() {
        // Get the device list
        if (devices.size() == 0) {
            throw NetworkException("no Infiniband devices available");
        } else if (devices.size() > 1) {
            throw NetworkException("more than 1 Infiniband devices available .. not handled right now");
        }

        // Get the verbs context
        context = devices[0]->open();

        // Create the protection domain
        protectionDomain = context->allocProtectionDomain();

        // Create receive queue
        ibv::srq::InitAttributes initAttributes(ibv::srq::Attributes(
                16351, // max_wr
                1 // max_sge
        ));

        sharedReceiveQueue = protectionDomain->createSrq(initAttributes);

        sharedCompletionQueuePair = make_unique<CompletionQueuePair>(*this);
    }

/// Get the LID
    uint16_t Network::getLID() {
        return context->queryPort(ibport).getLid();
    }

/// Print the capabilities of the RDMA host channel adapter
    void Network::printCapabilities() {
        // Get a list of all devices
        ibv::device::DeviceList deviceList{};

        // Open the first device
        auto context = deviceList[0]->open();

        // Query device attributes
        // auto device_attr = context->queryAttributes();

        // Print attributes
        std::cout << "[Device Information]" << std::endl;
        std::cout << std::left << std::setw(44) << "  Device Name: " << context->getDevice()->getName() << std::endl;
        std::cout << std::left << std::setw(44) << "  GUID: " << context->getDevice()->getGUID() << std::endl;
        /* TODO
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
            std::cout << "  The device supports modifying the maximum number of outstanding Work Requests of a QP"
                      << std::endl;
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
            std::cout
                    << "  The device supports changing the primary port number of a QP when transitioning from SQD to SQD state"
                    << std::endl;
        }
        if (device_attr.device_cap_flags & IBV_DEVICE_UD_AV_PORT_ENFORCE) {
            std::cout << "  The device supports AH port number enforcement" << std::endl;
        }
        if (device_attr.device_cap_flags & IBV_DEVICE_CURR_QP_STATE_MOD) {
            std::cout << "  The device supports the Current QP state modifier when calling ibv_modify_qp()"
                      << std::endl;
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
            std::cout << "  The device supports modifying the maximum number of outstanding Work Requests in an SRQ"
                      << std::endl;
        }
        if (device_attr.device_cap_flags & IBV_DEVICE_N_NOTIFY_CQ) {
            std::cout
                    << "  The device supports Requesting Completion notification when N completions were added (and not only one) to a CQ"
                    << std::endl;
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
        std::cout << std::setw(44) << "  Max total multicast QPs: " << device_attr.max_total_mcast_qp_attach
                  << std::endl;

        std::cout << "[Atomics]" << std::endl;
        switch (device_attr.atomic_cap) {
            case (IBV_ATOMIC_NONE):
                std::cout << "  Atomic operations aren’t supported at all" << std::endl;
                break;
            case (IBV_ATOMIC_HCA):
                std::cout << "  Atomicity is guaranteed between QPs on this device only" << std::endl;
                break;
            case (IBV_ATOMIC_GLOB):
                std::cout
                        << "  Atomicity is guaranteed between this device and any other component, such as CPUs and other devices"
                        << std::endl;
                break;
        }
        std::cout << std::setw(44) << "  Max outstanding reads/atomics per QP: " << device_attr.max_qp_rd_atom
                  << std::endl;
        std::cout << std::setw(44) << "  Resources for reads/atomics: " << device_attr.max_res_rd_atom << std::endl;
        std::cout << std::setw(44) << "  Max depth per QP read/atomic initiation: " << device_attr.max_qp_init_rd_atom
                  << std::endl;

        std::cout << "[Reliable Datagram]" << std::endl;
        std::cout << std::setw(44) << "  Max number of SGEs per QP: " << device_attr.max_sge_rd << std::endl;
        std::cout << std::setw(44) << "  Max number of EECs: " << device_attr.max_ee << std::endl;
        std::cout << std::setw(44) << "  Max number of RDDs: " << device_attr.max_rdd << std::endl;
        std::cout << std::setw(44) << "  Max outstanding reads/atomics per EEC: " << device_attr.max_ee_rd_atom
                  << std::endl;
        std::cout << std::setw(44) << "  Max depth per EEC read/atomic initiation: " << device_attr.max_ee_init_rd_atom
                  << std::endl;

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
        */
    }

    std::unique_ptr<ibv::memoryregion::MemoryRegion>
    Network::registerMr(void *addr, size_t length, std::initializer_list<ibv::AccessFlag> flags) {
        return protectionDomain->registerMemoryRegion(addr, length, flags);
    }

    RemoteMemoryRegion RemoteMemoryRegion::slice(size_t offset) {
        return RemoteMemoryRegion{this->address + offset, this->key};
    }
}
