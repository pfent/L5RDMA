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
public:
    void send(uint8_t *data, size_t length);

    std::vector<uint8_t> receive();

    RDMAMessageBuffer(size_t size, int sock);

private:
    const size_t size;
    RDMANetworking net;
    std::unique_ptr<volatile uint8_t[]> receiveBuffer;
    size_t currentReceive = 0;
    std::unique_ptr<uint8_t> sendBuffer;
    size_t currentSend = 0;
    rdma::MemoryRegion localSend;
    rdma::MemoryRegion localReceive;
    rdma::RemoteMemoryRegion remoteReceive;

    void writeToSendBuffer(uint8_t *data, size_t sizeToWrite);

    void readFromReceiveBuffer(uint8_t *whereTo, size_t sizeToRead);

    void zeroReceiveBuffer(size_t beginReceiveCount, size_t sizeToZero);
};

#endif //RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H
