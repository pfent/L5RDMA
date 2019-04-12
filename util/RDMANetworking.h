#ifndef L5RDMA_RDMANETWORKING_H
#define L5RDMA_RDMANETWORKING_H

#include "rdma/RcQueuePair.h"
#include "rdma/Network.hpp"
#include "rdma/CompletionQueuePair.hpp"

namespace l5 {
namespace util {
class Socket;
struct RDMANetworking {
    rdma::Network network;
    rdma::CompletionQueuePair completionQueue;
    rdma::RcQueuePair queuePair;

    /// Exchange the basic RDMA connection info for the network and queues
    explicit RDMANetworking(const Socket &sock);
};

struct RmrInfo {
    uint32_t bufferKey;
    uint32_t readPosKey;
    uintptr_t bufferAddress;
    uintptr_t readPosAddress;
};

void
receiveAndSetupRmr(const Socket &sock, ibv::memoryregion::RemoteAddress &buffer,
                   ibv::memoryregion::RemoteAddress &readPos);

void
sendRmrInfo(const Socket &sock, const ibv::memoryregion::MemoryRegion &buffer,
            const ibv::memoryregion::MemoryRegion &readPos);
} // namespace util
} // namespace l5
#endif //L5RDMA_RDMANETWORKING_H
