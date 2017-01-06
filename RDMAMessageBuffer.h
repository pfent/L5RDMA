#ifndef RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H
#define RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H

#include "rdma/Network.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/MemoryRegion.hpp"

struct RDMANetworking {
    rdma::Network network;
    rdma::CompletionQueuePair completionQueue;
    rdma::QueuePair queuePair;

    RDMANetworking(int sock);
};

class RDMAMessageBuffer {
    static const size_t validity = 0xDEADDEADBEEFBEEF;
public:
    void send(uint8_t *data, size_t length);

    std::vector<uint8_t> receive();

    RDMAMessageBuffer(size_t size, int sock);

private:
    const size_t size;
    RDMANetworking net;
    std::unique_ptr<volatile uint8_t[]> receiveBuffer;
    std::unique_ptr<uint8_t> sendBuffer;
    rdma::MemoryRegion localSend;
    rdma::MemoryRegion localReceive;
    rdma::RemoteMemoryRegion remoteReceive;
};

#endif //RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H
