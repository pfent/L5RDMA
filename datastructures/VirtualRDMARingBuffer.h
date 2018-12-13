#ifndef L5RDMA_VIRTUALRDMARINGBUFFER_H
#define L5RDMA_VIRTUALRDMARINGBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>
#include "util/RDMANetworking.h"
#include "util/virtualMemory.h"

namespace l5 {
namespace datastructure {

class VirtualRDMARingBuffer {
    static constexpr size_t validity = 0xDEADDEADBEEFBEEF;
    const size_t size;
    const size_t bitmask;
    util::RDMANetworking net;

    size_t messageCounter = 0;
    size_t sendPos = 0;
    std::atomic<size_t> localReadPos = 0;
    util::WraparoundBuffer sendBuf;
    rdma::MemoryRegion localSendMr;
    rdma::MemoryRegion localReadPosMr;

    std::atomic<size_t> remoteReadPos = 0;
    util::WraparoundBuffer receiveBuf;
    rdma::MemoryRegion localReceiveMr;
    rdma::MemoryRegion remoteReadPosMr;

    ibv::memoryregion::RemoteAddress remoteReceiveRmr{};
    ibv::memoryregion::RemoteAddress remoteReadPosRmr{};
public:
    /// Establish a shared memory region of size with the remote side of sock
    VirtualRDMARingBuffer(size_t size, const util::Socket &sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);

    /// send data via a lambda to enable zerocopy operation
    /// expected signature: [](uint8_t* begin) -> size_t
    template<typename SizeReturner>
    void send(SizeReturner &&doWork) {
        static_assert(std::is_unsigned_v<std::result_of_t<SizeReturner(uint8_t *)>>);
        const auto startOfWrite = sendPos & bitmask;
        auto sizePtr = reinterpret_cast<volatile size_t *>(&sendBuf.data.get()[startOfWrite]);
        auto begin = reinterpret_cast<volatile uint8_t *>(sizePtr + 1);

        // let the caller do the data stuff
        const size_t dataSize = doWork(begin);
        const auto sizeToWrite = sizeof(size) + dataSize + sizeof(validity);
        if (sizeToWrite > size) throw std::runtime_error{"data > buffersize!"};

        *sizePtr = dataSize;
        auto validityPtr = reinterpret_cast<volatile size_t *>(begin + dataSize);
        *validityPtr = validity;

        // actually send the message via rdma (similar to send)
        const auto sendSlice = localSendMr->getSlice(startOfWrite, sizeToWrite);
        const auto remoteSlice = remoteReceiveRmr.offset(startOfWrite);
        const auto shouldClearQueue = messageCounter % (4 * 1024) == 0;

        ibv::workrequest::Simple<ibv::workrequest::Write> wr;
        wr.setLocalAddress(sendSlice);
        wr.setRemoteAddress(remoteSlice);
        if (shouldClearQueue) {
            wr.setSignaled();
        }
        if (sendSlice.length <= net.queuePair.getMaxInlineSize()) {
            wr.setInline();
        }
        waitUntilSendFree(sizeToWrite);
        net.queuePair.postWorkRequest(wr);

        if (shouldClearQueue) {
            net.completionQueue.waitForCompletion();
        }
        ++messageCounter;

        // finally, update sendPos
        sendPos += sizeToWrite;
    }

    /// receive data via a lambda to enable zerocopy operation
    /// expected signature: [](const uint8_t* begin, const uint8_t* end) -> void
    template<typename RangeConsumer>
    void receive(RangeConsumer &&callback) {
        static_assert(std::is_void_v<std::result_of_t<RangeConsumer(const uint8_t *, const uint8_t *)>>);
        const auto lastReadPos = localReadPos.load();
        const auto startOfRead = lastReadPos & bitmask;

        size_t receiveSize;
        size_t checkMe;
        do {
            receiveSize = *reinterpret_cast<volatile size_t *>(&receiveBuf.data.get()[startOfRead]);
            checkMe = *reinterpret_cast<volatile size_t *>(&receiveBuf.data.get()[startOfRead + sizeof(size_t) +
                    receiveSize]);
        } while (checkMe != validity);

        const auto begin = &receiveBuf.data.get()[startOfRead + sizeof(receiveSize)];
        const auto end = begin + receiveSize;

        // let the caller do the data stuff
        callback(begin, end);

        const auto totalSizeRead = sizeof(receiveSize) + receiveSize + sizeof(validity);
        std::fill(&receiveBuf.data.get()[startOfRead], &receiveBuf.data.get()[startOfRead + totalSizeRead], 0);

        localReadPos.store(lastReadPos + totalSizeRead, std::memory_order_release);
    }

private:
    void waitUntilSendFree(size_t sizeToWrite);
};
} // namespace datastructure
} // namespace l5

#endif //L5RDMA_VIRTUALRDMARINGBUFFER_H
