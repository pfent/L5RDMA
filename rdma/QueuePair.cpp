#include "QueuePair.hpp"
#include "Network.hpp"
#include "CompletionQueuePair.hpp"
#include <iomanip>

using namespace std;
namespace rdma {
    QueuePair::QueuePair(Network &network, ibv::queuepair::Type type)
            : QueuePair(network, type, network.sharedCompletionQueuePair, *network.sharedReceiveQueue) {}

    QueuePair::QueuePair(Network &network, ibv::queuepair::Type type, ibv::srq::SharedReceiveQueue &receiveQueue)
            : QueuePair(network, type, network.sharedCompletionQueuePair, receiveQueue) {}

    QueuePair::QueuePair(Network &network, ibv::queuepair::Type type, CompletionQueuePair &completionQueuePair)
            : QueuePair(network, type, completionQueuePair, *network.sharedReceiveQueue) {}

    QueuePair::QueuePair(Network &network, ibv::queuepair::Type type, CompletionQueuePair &completionQueuePair,
                         ibv::srq::SharedReceiveQueue &receiveQueue)
            : network(network), completionQueuePair(completionQueuePair),
              type(type) {
        ibv::queuepair::InitAttributes queuePairAttributes{};
        queuePairAttributes.setContext(context);
        // CQ to be associated with the Send Queue (SQ)
        queuePairAttributes.setSendCompletionQueue(*completionQueuePair.sendQueue);
        // CQ to be associated with the Receive Queue (RQ)
        queuePairAttributes.setRecvCompletionQueue(*completionQueuePair.receiveQueue);
        // SRQ handle if QP is to be associated with an SRQ, otherwise NULL
        queuePairAttributes.setSharedReceiveQueue(receiveQueue);
        ibv::queuepair::Capabilities capabilities{};
        capabilities.max_send_wr = maxOutstandingSendWrs;
        capabilities.max_recv_wr = maxOutstandingRecvWrs;
        capabilities.max_send_sge = maxSlicesPerSendWr;
        capabilities.max_recv_sge = maxSlicesPerRecvWr;
        capabilities.max_inline_data = maxInlineSize;
        queuePairAttributes.setCapabilities(capabilities);
        queuePairAttributes.setType(type);
        queuePairAttributes.setSignalAll(signalAll);

        // Create queue pair
        qp = network.protectionDomain->createQueuePair(queuePairAttributes);
    }

    uint32_t QueuePair::getQPN() {
        return qp->getNum();
    }

    void QueuePair::connectRC(const Address &address, uint8_t port, uint8_t retryCount) {
        using Access = ibv::AccessFlag;
        using Mod = ibv::queuepair::AttrMask;

        {   // First initialize the the QP
            ibv::queuepair::Attributes attributes{};
            attributes.setQpState(ibv::queuepair::State::INIT);
            attributes.setPkeyIndex(0); // Partition the queue pair belongs to
            attributes.setPortNum(port); // The local physical port
            // Allowed access flags of the remote operations for incoming packets (i.e., none, RDMA read, RDMA write, or atomics)
            attributes.setQpAccessFlags({Access::REMOTE_WRITE, Access::REMOTE_READ, Access::REMOTE_ATOMIC});

            qp->modify(attributes, {Mod::STATE, Mod::PKEY_INDEX, Mod::PORT, Mod::ACCESS_FLAGS});
        }

        {   // RTR (ready to receive)
            ibv::queuepair::Attributes attributes{};
            attributes.setQpState(ibv::queuepair::State::RTR);
            attributes.setPathMtu(ibv::Mtu::_4096);             // Maximum payload size
            attributes.setDestQpNum(address.qpn);               // The remote QP number
            attributes.setRqPsn(0);                             // The packet sequence number of received packets
            attributes.setMaxDestRdAtomic(16); // The number of outstanding RDMA reads & atomic operations (destination)
            attributes.setMinRnrTimer(12);                      // The time before a RNR NACK is sent
            ibv::ah::Attributes ahAttributes{};
            ahAttributes.setIsGlobal(false);                    // Whether there is a global routing header
            ahAttributes.setDlid(address.lid);                  // The LID of the remote host
            ahAttributes.setSl(0);                              // The service level (which determines the virtual lane)
            ahAttributes.setSrcPathBits(0);                     // Use the port base LID
            ahAttributes.setPortNum(port);                      // The local physical port
            attributes.setAhAttr(ahAttributes);

            qp->modify(attributes, {Mod::STATE, Mod::AV, Mod::PATH_MTU, Mod::DEST_QPN, Mod::RQ_PSN,
                                    Mod::MAX_DEST_RD_ATOMIC, Mod::MIN_RNR_TIMER});
        }

        {   // RTS (ready to send)
            ibv::queuepair::Attributes attributes{};
            attributes.setQpState(ibv::queuepair::State::RTS);
            attributes.setSqPsn(0);             // The packet sequence number of sent packets
            attributes.setTimeout(0);           // The minimum timeout before retransmitting the packet (0 = infinite)
            attributes.setRetryCnt(retryCount); // How often to retry sending (7 = infinite)
            attributes.setRnrRetry(retryCount); // How often to retry sending when RNR NACK was received (7 = infinite)
            attributes.setMaxRdAtomic(128);     // The number of outstanding RDMA reads & atomic operations (initiator)
            qp->modify(attributes, {Mod::STATE, Mod::TIMEOUT, Mod::RETRY_CNT, Mod::RNR_RETRY, Mod::SQ_PSN,
                                    Mod::MAX_QP_RD_ATOMIC});
        }
    }

    void QueuePair::connectUD(const Address &, uint8_t port, uint32_t packetSequenceNumber) {
        using Mod = ibv::queuepair::AttrMask;

        {
            ibv::queuepair::Attributes attr{};
            attr.setQpState(ibv::queuepair::State::INIT);
            attr.setPkeyIndex(0);
            attr.setPortNum(port);
            attr.setQkey(0x22222222);

            qp->modify(attr, {Mod::STATE, Mod::PKEY_INDEX, Mod::PORT, Mod::QKEY});
        }

        {   // RTR
            ibv::queuepair::Attributes attr{};
            attr.setQpState(ibv::queuepair::State::RTR);

            qp->modify(attr, {Mod::STATE});
        }

        {   // RTS
            ibv::queuepair::Attributes attr{};
            attr.setQpState(ibv::queuepair::State::RTS);
            attr.setSqPsn(packetSequenceNumber);

            qp->modify(attr, {Mod::STATE, Mod::SQ_PSN});
        }
    }

    void QueuePair::postWorkRequest(ibv::workrequest::SendWr &workRequest) {
        ibv::workrequest::SendWr *badWorkRequest = nullptr;
        qp->postSend(workRequest, badWorkRequest);
    }

    namespace { // Anonymous helper namespace
        string queuePairAccessFlagsToString(ibv::queuepair::Attributes attr) {
            string result;
            if (attr.hasQpAccessFlags(ibv::AccessFlag::REMOTE_WRITE))
                result += "IBV_ACCESS_REMOTE_WRITE, ";
            if (attr.hasQpAccessFlags(ibv::AccessFlag::REMOTE_READ))
                result += "IBV_ACCESS_REMOTE_READ, ";
            if (attr.hasQpAccessFlags(ibv::AccessFlag::REMOTE_ATOMIC))
                result += "IBV_ACCESS_REMOTE_ATOMIC, ";
            return result;
        }
    } // end of anonymous helper namespace

    void QueuePair::printQueuePairDetails() const {
        using Mask = ibv::queuepair::AttrMask;

        auto attr = qp->query({Mask::STATE, Mask::CUR_STATE, Mask::EN_SQD_ASYNC_NOTIFY, Mask::ACCESS_FLAGS,
                               Mask::PKEY_INDEX, Mask::PORT, Mask::QKEY, Mask::AV, Mask::PATH_MTU, Mask::TIMEOUT,
                               Mask::RETRY_CNT, Mask::RNR_RETRY, Mask::RQ_PSN, Mask::MAX_QP_RD_ATOMIC, Mask::ALT_PATH,
                               Mask::MIN_RNR_TIMER, Mask::SQ_PSN, Mask::MAX_DEST_RD_ATOMIC, Mask::PATH_MIG_STATE,
                               Mask::CAP, Mask::DEST_QPN});

        cout << "[State of QP " << qp.get() << "]" << endl;
        cout << endl;
        cout << left << setw(44) << "qp_state:" << to_string(attr.getQpState()) << endl;
        cout << left << setw(44) << "cur_qp_state:" << to_string(attr.getCurQpState()) << endl;
        cout << left << setw(44) << "path_mtu:" << to_string(attr.getPathMtu()) << endl;
        cout << left << setw(44) << "path_mig_state:" << to_string(attr.getPathMigState()) << endl;
        cout << left << setw(44) << "qkey:" << attr.getQkey() << endl;
        cout << left << setw(44) << "rq_psn:" << attr.getRqPsn() << endl;
        cout << left << setw(44) << "sq_psn:" << attr.getSqPsn() << endl;
        cout << left << setw(44) << "dest_qp_num:" << attr.getDestQpNum() << endl;
        cout << left << setw(44) << "qp_access_flags:" << queuePairAccessFlagsToString(attr) << endl;
        cout << left << setw(44) << "cap:" << "<not impl>" << endl;
        cout << left << setw(44) << "ah_attr:" << "<not impl>" << endl;
        cout << left << setw(44) << "alt_ah_attr:" << "<not impl>" << endl;
        cout << left << setw(44) << "pkey_index:" << attr.getPkeyIndex() << endl;
        cout << left << setw(44) << "alt_pkey_index:" << attr.getAltPkeyIndex() << endl;
        cout << left << setw(44) << "en_sqd_async_notify:" << static_cast<int>(attr.getEnSqdAsyncNotify()) << endl;
        cout << left << setw(44) << "sq_draining:" << static_cast<int>(attr.getSqDraining()) << endl;
        cout << left << setw(44) << "max_rd_atomic:" << static_cast<int>(attr.getMaxRdAtomic()) << endl;
        cout << left << setw(44) << "max_dest_rd_atomic:" << static_cast<int>(attr.getMaxDestRdAtomic()) << endl;
        cout << left << setw(44) << "min_rnr_timer:" << static_cast<int>(attr.getMinRnrTimer()) << endl;
        cout << left << setw(44) << "port_num:" << static_cast<int>(attr.getPortNum()) << endl;
        cout << left << setw(44) << "timeout:" << static_cast<int>(attr.getTimeout()) << endl;
        cout << left << setw(44) << "retry_cnt:" << static_cast<int>(attr.getRetryCnt()) << endl;
        cout << left << setw(44) << "rnr_retry:" << static_cast<int>(attr.getRnrRetry()) << endl;
        cout << left << setw(44) << "alt_port_num:" << static_cast<int>(attr.getAltPortNum()) << endl;
        cout << left << setw(44) << "alt_timeout:" << static_cast<int>(attr.getAltTimeout()) << endl;
    }

    uint32_t QueuePair::getMaxInlineSize() const {
        return maxInlineSize;
    }

    void QueuePair::connect(const Address &address) {
        switch (type) {
            case ibv::queuepair::Type::RC:
                return connectRC(address, network.ibport);
            case ibv::queuepair::Type::UD:
                return connectUD(address, network.ibport);
            default:
                throw;
        }
    }
} // End of namespace rdma
