#include "UdQueuePair.h"

void rdma::UdQueuePair::connect(const rdma::Address &) {
    connect(defaultPort);
}

void rdma::UdQueuePair::connect(uint8_t port, uint32_t packetSequenceNumber) {
    using Mod = ibv::queuepair::AttrMask;

    {
        ibv::queuepair::Attributes attr{};
        attr.setQpState(ibv::queuepair::State::INIT);
        attr.setPkeyIndex(0);
        attr.setPortNum(port);
        attr.setQkey(0x22222222); // todo: bad magic constant

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
