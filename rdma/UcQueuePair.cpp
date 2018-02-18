#include "UcQueuePair.h"

void rdma::UcQueuePair::connect(const Address &address) {
    connect(address, defaultPort);
}

void rdma::UcQueuePair::connect(const Address &address, uint8_t port) {
    using Access = ibv::AccessFlag;
    using Mod = ibv::queuepair::AttrMask;

    {   // INIT
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::INIT);
        attributes.setPkeyIndex(0);
        attributes.setPortNum(port);
        attributes.setQpAccessFlags({Access::REMOTE_WRITE});

        qp->modify(attributes, {Mod::STATE, Mod::PKEY_INDEX, Mod::PORT, Mod::ACCESS_FLAGS});
    }

    {   // Ready to receive
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::RTR);
        attributes.setPathMtu(ibv::Mtu::_4096);
        attributes.setDestQpNum(address.qpn);
        attributes.setRqPsn(0);
        ibv::ah::Attributes ahAttributes{};
        ahAttributes.setIsGlobal(false);
        ahAttributes.setDlid(address.lid);
        ahAttributes.setSl(0);
        ahAttributes.setSrcPathBits(0);
        ahAttributes.setPortNum(port);
        attributes.setAhAttr(ahAttributes);

        qp->modify(attributes, {Mod::STATE, Mod::AV, Mod::PATH_MTU, Mod::DEST_QPN, Mod::RQ_PSN});
    }

    {   // Ready to send
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::RTS);
        attributes.setSqPsn(0);

        qp->modify(attributes, {Mod::STATE, Mod::SQ_PSN});
    }
}
