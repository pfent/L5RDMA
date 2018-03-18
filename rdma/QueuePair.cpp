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
            : defaultPort(network.ibport), receiveQueue(receiveQueue) {
        ibv::queuepair::InitAttributes queuePairAttributes{};
        queuePairAttributes.setContext(context);
        // CQ to be associated with the Send Queue (SQ)
        queuePairAttributes.setSendCompletionQueue(completionQueuePair.getSendQueue());
        // CQ to be associated with the Receive Queue (RQ)
        queuePairAttributes.setRecvCompletionQueue(completionQueuePair.getReceiveQueue());
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

    void QueuePair::postWorkRequest(ibv::workrequest::SendWr &workRequest) {
        ibv::workrequest::SendWr *badWorkRequest = nullptr;
        qp->postSend(workRequest, badWorkRequest);
    }

    void QueuePair::postRecvRequest(ibv::workrequest::Recv &recvRequest) {
        ibv::workrequest::Recv *badWorkRequest = nullptr;
        receiveQueue.postRecv(recvRequest, badWorkRequest);
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
        cout << left << setw(44) << "cur_qp_state:" << to_string(attr.getQpState()) << endl;
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

    QueuePair::~QueuePair() = default;
} // End of namespace rdma
