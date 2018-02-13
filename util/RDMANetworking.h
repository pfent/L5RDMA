#ifndef EXCHANGABLETRANSPORTS_RDMANETWORKING_H
#define EXCHANGABLETRANSPORTS_RDMANETWORKING_H

#include <exchangeableTransports/rdma/RcQueuePair.h>
#include "exchangeableTransports/rdma/Network.hpp"
#include "exchangeableTransports/rdma/CompletionQueuePair.hpp"
#include "tcpWrapper.h"

struct RDMANetworking {
    rdma::Network network;
    rdma::CompletionQueuePair completionQueue;
    rdma::RcQueuePair queuePair;

    /// Exchange the basic RDMA connection info for the network and queues
    explicit RDMANetworking(int sock);
};

struct RmrInfo {
    uint32_t bufferKey;
    uint32_t readPosKey;
    uintptr_t bufferAddress;
    uintptr_t readPosAddress;
};

void receiveAndSetupRmr(int sock, rdma::RemoteMemoryRegion &buffer, rdma::RemoteMemoryRegion &readPos);

void
sendRmrInfo(int sock, const ibv::memoryregion::MemoryRegion &buffer, const ibv::memoryregion::MemoryRegion &readPos);

#endif //EXCHANGABLETRANSPORTS_RDMANETWORKING_H
