#include "Network.hpp"
#include "CompletionQueuePair.hpp"
#include <iostream>
#include <iomanip>
#include "NetworkException.h"

using namespace std;

static std::unique_ptr<ibv::context::Context> openUnambigousDevice(ibv::device::DeviceList &devices) {
    if (devices.size() == 0) {
        throw rdma::NetworkException("no Infiniband devices available");
    } else if (devices.size() > 1) {
        throw rdma::NetworkException("more than 1 Infiniband devices available .. not handled right now");
    }
    return devices[0]->open();
}

namespace rdma {
    ostream &operator<<(ostream &os, const ibv::memoryregion::RemoteAddress &remoteMemoryRegion) {
        return os << "address=" << reinterpret_cast<void *>(remoteMemoryRegion.address) << " key="
                  << remoteMemoryRegion.rkey;
    }

    ostream &operator<<(ostream &os, const Address &address) {
        return os << "lid=" << address.lid << ", qpn=" << address.qpn;
    }

    Network::Network() : devices(), context(openUnambigousDevice(devices)), sharedCompletionQueuePair(*context) {
        // Create the protection domain
        protectionDomain = context->allocProtectionDomain();

        // Create receive queue
        ibv::srq::InitAttributes initAttributes(ibv::srq::Attributes(maxWr, maxSge));
        sharedReceiveQueue = protectionDomain->createSrq(initAttributes);
    }

    /// Get the LID
    uint16_t Network::getLID() {
        return context->queryPort(ibport).getLid();
    }

    /// Get the GID
    ibv::Gid Network::getGID() {
        return context->queryGid(ibport, 0);
    }

    /// Print the capabilities of the RDMA host channel adapter
    void Network::printCapabilities() {
        using Cap = ibv::device::CapabilityFlag;
        // Get a list of all devices
        for (auto device : devices) {
            // Open the device
            auto context = device->open();

            // Query device attributes
            const auto device_attr = context->queryAttributes();

            // Print attributes
            cout << "[Device Information]" << '\n';
            cout << left << setw(44) << "  Device Name: " << context->getDevice()->getName() << '\n';
            cout << left << setw(44) << "  GUID: " << context->getDevice()->getGUID() << '\n';
            cout << left << setw(44) << "  Vendor ID: " << device_attr.getVendorId() << '\n';
            cout << left << setw(44) << "  Vendor Part ID: " << device_attr.getVendorPartId() << '\n';
            cout << left << setw(44) << "  Hardware Version: " << device_attr.getHwVer() << '\n';
            cout << left << setw(44) << "  Firmware Version: " << device_attr.getFwVer() << '\n';
            cout << left << setw(44) << "  Physical Ports: " << device_attr.getPhysPortCnt() << '\n';
            cout << left << setw(44) << "  CA ACK Delay: " << device_attr.getLocalCaAckDelay() << '\n';

            cout << "[Memory]" << '\n';
            cout << left << setw(44) << "  Max MR size: " << device_attr.getMaxMrSize() << '\n';
            cout << left << setw(44) << "  Max page size: " << device_attr.getPageSizeCap() << '\n';

            cout << "[Capabilities]" << '\n';
            if (device_attr.hasCapability(Cap::RESIZE_MAX_WR)) {
                cout << "  The device supports modifying the maximum number of outstanding Work Requests of a QP"
                     << '\n';
            }
            if (device_attr.hasCapability(Cap::BAD_PKEY_CNTR)) {
                cout << "  The device supports bad P_Key counting for each port" << '\n';
            }
            if (device_attr.hasCapability(Cap::BAD_QKEY_CNTR)) {
                cout << "  The device supports P_Key violations counting for each port" << '\n';
            }
            if (device_attr.hasCapability(Cap::RAW_MULTI)) {
                cout << "  The device supports raw packet multicast" << '\n';
            }
            if (device_attr.hasCapability(Cap::AUTO_PATH_MIG)) {
                cout << "  The device supports automatic path migration" << '\n';
            }
            if (device_attr.hasCapability(Cap::CHANGE_PHY_PORT)) {
                cout << "  The device supports changing the primary port number of a QP when transitioning from SQD to "
                        "SQD state" << '\n';
            }
            if (device_attr.hasCapability(Cap::UD_AV_PORT_ENFORCE)) {
                cout << "  The device supports AH port number enforcement" << '\n';
            }
            if (device_attr.hasCapability(Cap::CURR_QP_STATE_MOD)) {
                cout << "  The device supports the Current QP state modifier when calling ibv_modify_qp()" << '\n';
            }
            if (device_attr.hasCapability(Cap::SHUTDOWN_PORT)) {
                cout << "  The device supports shutdown port" << '\n';
            }
            if (device_attr.hasCapability(Cap::INIT_TYPE)) {
                cout << "  The device supports setting InitType and InitTypeReply" << '\n';
            }
            if (device_attr.hasCapability(Cap::PORT_ACTIVE_EVENT)) {
                cout << "  The device supports the IBV_EVENT_PORT_ACTIVE event generation" << '\n';
            }
            if (device_attr.hasCapability(Cap::SYS_IMAGE_GUID)) {
                cout << "  The device supports System Image GUID" << '\n';
            }
            if (device_attr.hasCapability(Cap::RC_RNR_NAK_GEN)) {
                cout << "  The device supports RNR-NAK generation for RC QPs" << '\n';
            }
            if (device_attr.hasCapability(Cap::SRQ_RESIZE)) {
                cout << "  The device supports modifying the maximum number of outstanding Work Requests in an SRQ"
                     << '\n';
            }
            if (device_attr.hasCapability(Cap::N_NOTIFY_CQ)) {
                cout << "  The device supports Requesting Completion notification when N completions were added (and "
                        "not only one) to a CQ" << '\n';
            }

            cout << "[Resources]" << '\n';
            cout << setw(44) << "  Max number of QPs: " << device_attr.getMaxQp() << '\n';
            cout << setw(44) << "  Max number of WRs per Queue: " << device_attr.getMaxQpWr() << '\n';
            cout << setw(44) << "  Max number of SGE per WR: " << device_attr.getMaxSge() << '\n';
            cout << setw(44) << "  Max number of CQs: " << device_attr.getMaxCq() << '\n';
            cout << setw(44) << "  Max number of CQEs per CQ: " << device_attr.getMaxCqe() << '\n';
            cout << setw(44) << "  Max number of PDs: " << device_attr.getMaxPd() << '\n';
            cout << setw(44) << "  Max number of MRs: " << device_attr.getMaxMr() << '\n';
            cout << setw(44) << "  Max number of AHs: " << device_attr.getMaxAh() << '\n';
            cout << setw(44) << "  Max number of partitions: " << device_attr.getMaxPkeys() << '\n';

            cout << "[Multicast]" << '\n';
            cout << setw(44) << "  Max multicast groups: " << device_attr.getMaxMcastGrp() << '\n';
            cout << setw(44) << "  Max QPs per multicast group: " << device_attr.getMaxMcastQpAttach() << '\n';
            cout << setw(44) << "  Max total multicast QPs: " << device_attr.getMaxTotalMcastQpAttach() << '\n';

            cout << "[Atomics]" << '\n';
            switch (device_attr.getAtomicCap()) {
                case (ibv::device::AtomicCapabilities::NONE):
                    cout << "  Atomic operations aren’t supported at all" << '\n';
                    break;
                case (ibv::device::AtomicCapabilities::HCA):
                    cout << "  Atomicity is guaranteed between QPs on this device only" << '\n';
                    break;
                case (ibv::device::AtomicCapabilities::GLOB):
                    cout << "  Atomicity is guaranteed between this device and any other component, such as CPUs and "
                            "other devices" << '\n';
                    break;
            }
            cout << setw(44) << "  Max outstanding reads/atomics per QP: " << device_attr.getMaxQpRdAtom() << '\n';
            cout << setw(44) << "  Resources for reads/atomics: " << device_attr.getMaxResRdAtom() << '\n';
            cout << setw(44) << "  Max depth per QP read/atomic initiation: " << device_attr.getMaxQpInitRdAtom()
                 << '\n';

            cout << "[Reliable Datagram]" << '\n';
            cout << setw(44) << "  Max number of SGEs per QP: " << device_attr.getMaxSgeRd() << '\n';
            cout << setw(44) << "  Max number of EECs: " << device_attr.getMaxEe() << '\n';
            cout << setw(44) << "  Max number of RDDs: " << device_attr.getMaxRdd() << '\n';
            cout << setw(44) << "  Max outstanding reads/atomics per EEC: " << device_attr.getMaxEeRdAtom() << '\n';
            cout << setw(44) << "  Max depth per EEC read/atomic initiation: "
                 << device_attr.getMaxEeInitRdAtom() << '\n';

            cout << "[Memory Windows]" << '\n';
            cout << setw(44) << "  Max number of MWs: " << device_attr.getMaxMw() << '\n';

            cout << "[Fast Memory Registration]" << '\n';
            cout << setw(44) << "  Max number of FMRs: " << device_attr.getMaxFmr() << '\n';
            cout << setw(44) << "  Max number of maps per FMR: " << device_attr.getMaxMapPerFmr() << '\n';

            cout << "[Shared Receive Queues]" << '\n';
            cout << setw(44) << "  Max number of SRQs: " << device_attr.getMaxSrq() << '\n';
            cout << setw(44) << "  Max number of WR per SRQ: " << device_attr.getMaxSrqWr() << '\n';
            cout << setw(44) << "  Max number of SGEs per WR: " << device_attr.getMaxSrqSge() << '\n';

            cout << "[Raw]" << '\n';
            cout << setw(44) << "  Max number of IPv6 QPs: " << device_attr.getMaxRawIpv6Qp() << '\n';
            cout << setw(44) << "  Max number of Ethertype QPs: " << device_attr.getMaxRawEthyQp() << endl;
        }
    }

    unique_ptr<ibv::memoryregion::MemoryRegion>
    Network::registerMr(void *addr, size_t length, initializer_list<ibv::AccessFlag> flags) {
        return protectionDomain->registerMemoryRegion(addr, length, flags);
    }

    CompletionQueuePair Network::newCompletionQueuePair() {
        return CompletionQueuePair(*context);
    }

    ibv::protectiondomain::ProtectionDomain &Network::getProtectionDomain() {
        return *protectionDomain;
    }

    CompletionQueuePair &Network::getSharedCompletionQueue() {
        return sharedCompletionQueuePair;
    }
}
