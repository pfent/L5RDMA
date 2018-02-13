#ifndef EXCHANGABLETRANSPORTS_UCQUEUEPAIR_H
#define EXCHANGABLETRANSPORTS_UCQUEUEPAIR_H

#include "QueuePair.hpp"

namespace rdma {
    class UcQueuePair : public QueuePair {
    public:
        explicit UcQueuePair(Network& network) : QueuePair(network, ibv::queuepair::Type::UC) {}

        UcQueuePair(Network &network, CompletionQueuePair &completionQueuePair) :
        QueuePair(network, ibv::queuepair::Type::UC, completionQueuePair) {}

        void connect(const Address & address) override;

        /// Similar to RcQueuePair::connect(), just without retry and atomic settings
        void connect(const Address &address, uint8_t port);
    };
}

#endif //EXCHANGABLETRANSPORTS_UCQUEUEPAIR_H
