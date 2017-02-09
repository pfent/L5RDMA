#ifndef RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H
#define RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H

#include <atomic>
#include "rdma/Network.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/MemoryRegion.hpp"

struct RDMANetworking {
    rdma::Network network;
    rdma::CompletionQueuePair completionQueue;
    rdma::QueuePair queuePair;

    /// Exchange the basic RDMA connection info for the network and queues
    RDMANetworking(int sock);
};

class RDMAMessageBuffer {
public:

    /// Send data to the remote site
    void send(const uint8_t *data, size_t length);

    void send(const uint8_t *data, size_t length, bool inln);

    /// Receive data to a freshly allocated data vector
    std::vector<uint8_t> receive();

    /// Receive to a specific memory region with at last maxSize
    size_t receive(void *whereTo, size_t maxSize);

    /// Construct a message buffer of the given size, exchanging RDMA networking information over the given socket
    /// size _must_ be a power of 2.
    RDMAMessageBuffer(size_t size, int sock);

    /// whether there is data to be read non-blockingly
    bool hasData();

private:
    const size_t size;
    const size_t bitmask;
    RDMANetworking net;
    std::unique_ptr<volatile uint8_t[]> receiveBuffer;
    std::atomic<size_t> readPos{0};
    std::unique_ptr<uint8_t[]> sendBuffer;
    size_t sendPos = 0;
    volatile size_t currentRemoteReceive = 0;
    rdma::MemoryRegion localSend;
    rdma::MemoryRegion localReceive;
    rdma::MemoryRegion localReadPos;
    rdma::MemoryRegion localCurrentRemoteReceive;
    rdma::RemoteMemoryRegion remoteReceive;
    rdma::RemoteMemoryRegion remoteReadPos;

    void writeToSendBuffer(const uint8_t *data, size_t sizeToWrite);

    void readFromReceiveBuffer(size_t readPos, uint8_t *whereTo, size_t sizeToRead);

    void zeroReceiveBuffer(size_t beginReceiveCount, size_t sizeToZero);
};

#endif //RDMA_HASH_MAP_RDMAMESSAGEBUFFER_H
