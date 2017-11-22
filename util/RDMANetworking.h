#ifndef EXCHANGABLETRANSPORTS_RDMANETWORKING_H
#define EXCHANGABLETRANSPORTS_RDMANETWORKING_H

#include "exchangeableTransports/rdma/Network.hpp"
#include "exchangeableTransports/rdma/CompletionQueuePair.hpp"
#include "exchangeableTransports/rdma/QueuePair.hpp"
#include "exchangeableTransports/rdma/MemoryRegion.hpp"
#include "tcpWrapper.h"

struct RDMANetworking {
    rdma::Network network;
    rdma::CompletionQueuePair completionQueue;
    rdma::QueuePair queuePair;

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

void sendRmrInfo(int sock, const rdma::MemoryRegion &buffer, const rdma::MemoryRegion &readPos);

#endif //EXCHANGABLETRANSPORTS_RDMANETWORKING_H
