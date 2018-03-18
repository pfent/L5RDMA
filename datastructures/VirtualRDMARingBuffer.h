#ifndef EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
#define EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>
#include "util/RDMANetworking.h"
#include "util/virtualMemory.h"

class VirtualRDMARingBuffer {
    static constexpr size_t validity = 0xDEADDEADBEEFBEEF;
    const size_t size;
    const size_t bitmask;
    RDMANetworking net;

    size_t messageCounter = 0;
    size_t sendPos = 0;
    std::atomic<size_t> localReadPos = 0;
    WraparoundBuffer sendBuf;
    rdma::MemoryRegion localSendMr;
    rdma::MemoryRegion localReadPosMr;

    std::atomic<size_t> remoteReadPos = 0;
    WraparoundBuffer receiveBuf;
    rdma::MemoryRegion localReceiveMr;
    rdma::MemoryRegion remoteReadPosMr;

    ibv::memoryregion::RemoteAddress remoteReceiveRmr{};
    ibv::memoryregion::RemoteAddress remoteReadPosRmr{};
public:
    /// Establish a shared memory region of size with the remote side of sock
    VirtualRDMARingBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);

private:
    void waitUntilSendFree(size_t sizeToWrite);
};

#endif //EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
