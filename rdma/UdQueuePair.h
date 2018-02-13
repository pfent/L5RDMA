#ifndef EXCHANGABLETRANSPORTS_UDQUEUEPAIR_H
#define EXCHANGABLETRANSPORTS_UDQUEUEPAIR_H

#include "QueuePair.hpp"

namespace rdma {
    class UdQueuePair : public QueuePair {
    public:
        explicit UdQueuePair(Network &network) : QueuePair(network, ibv::queuepair::Type::UD) {}

        void connect(const Address & address) override;

        void connect(uint8_t port, uint32_t packetSequenceNumber = 0);
    };
}

#endif //EXCHANGABLETRANSPORTS_UDQUEUEPAIR_H
