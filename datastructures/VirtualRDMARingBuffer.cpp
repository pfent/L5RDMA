#include "VirtualRDMARingBuffer.h"
#include <exchangeableTransports/rdma/WorkRequest.hpp>
#include <iostream>

using Perm = rdma::MemoryRegion::Permission;
using namespace std::string_literals;

constexpr auto localRw = Perm::LocalWrite | Perm::RemoteWrite;

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, int sock) :
        size(size), bitmask(size - 1), net(sock),
        sendBuf(mmapSharedRingBuffer("/rdmaLocal"s + std::tmpnam(nullptr), size, true)),
        // Since we mapped twice the virtual memory, we can create memory regions of twice the size of the actual buffer
        localSendMr(sendBuf.get(), size * 2, net.network.getProtectionDomain(), Perm::None),
        localReadPosMr(&localReadPos, sizeof(localReadPos), net.network.getProtectionDomain(), Perm::RemoteRead),
        receiveBuf(mmapSharedRingBuffer("/rdmaRemote"s + std::tmpnam(nullptr), size, true)),
        localReceiveMr(receiveBuf.get(), size * 2, net.network.getProtectionDomain(), localRw),
        remoteReadPosMr(&remoteReadPos, sizeof(remoteReadPos), net.network.getProtectionDomain(), Perm::LocalWrite) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    sendRmrInfo(sock, localReceiveMr, localReadPosMr);
    receiveAndSetupRmr(sock, remoteReceiveRmr, remoteReadPosRmr);
}

void VirtualRDMARingBuffer::send(const uint8_t *data, size_t length) {
    const auto sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw std::runtime_error{"data > buffersize!"};

    const auto startOfWrite = sendPos & bitmask;
    auto whereToWrite = startOfWrite;
    const auto write = [&](auto what, auto howManyBytes) {
        auto whatPtr = reinterpret_cast<const uint8_t *>(what);
        volatile auto dest = &sendBuf.get()[whereToWrite];
        std::copy(whatPtr, &whatPtr[howManyBytes], dest);
        whereToWrite += howManyBytes;
    };

    waitUntilSendFree(sizeToWrite);

    // first write the data
    write(&length, sizeof(length));
    write(data, length);
    write(&validity, sizeof(validity));

    // then request it to be sent via RDMA
    const auto sendSlice = localSendMr.slice(startOfWrite, sizeToWrite);
    const auto remoteSlice = remoteReceiveRmr.slice(startOfWrite);
    // occasionally clear the queue (this can probably also happen only every 16k times)
    // aka "selective signaling"
    const auto shouldClearQueue = messageCounter % (4 * 1024) == 0;
    rdma::WriteWorkRequestBuilder(sendSlice, remoteSlice, shouldClearQueue)
            .setInline(sendSlice.size <= net.queuePair.getMaxInlineSize())
            .send(net.queuePair);
    if (shouldClearQueue) {
        net.completionQueue.waitForCompletion();
    }
    ++messageCounter;

    // finally, update sendPos
    sendPos += sizeToWrite;
}

size_t VirtualRDMARingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto maxSizeToRead = sizeof(maxSize) + maxSize + sizeof(validity);
    if (maxSizeToRead > size) throw std::runtime_error{"receiveSize > buffersize!"};

    const auto lastReadPos = localReadPos.load();
    const auto startOfRead = lastReadPos & bitmask;
    const auto readFromBuffer = [&](auto fromOffset, auto dest, auto howManyBytes) {
        volatile auto begin = &receiveBuf.get()[fromOffset];
        volatile auto end = &receiveBuf.get()[fromOffset + howManyBytes];
        std::copy(begin, end, reinterpret_cast<uint8_t *>(dest));
    };

    size_t receiveSize;
    size_t checkMe;
    do {
        receiveSize = *reinterpret_cast<volatile size_t *>(&receiveBuf.get()[startOfRead]);
        checkMe = *reinterpret_cast<volatile size_t *>(&receiveBuf.get()[startOfRead + sizeof(size_t) + receiveSize]);
    } while (checkMe != validity);
    // There might be a small chance, the length hadn't been written fully, yet and the validity somehow appeared in the data

    if (receiveSize > maxSize) {
        throw std::runtime_error{"plz only read whole messages for now!"}; // probably buffer partially read msgs
    }

    readFromBuffer(startOfRead + sizeof(receiveSize), whereTo, receiveSize);

    const auto totalSizeRead = sizeof(receiveSize) + receiveSize + sizeof(validity);
    std::fill(&receiveBuf.get()[startOfRead], &receiveBuf.get()[startOfRead + totalSizeRead], 0);

    localReadPos.store(lastReadPos + totalSizeRead, std::memory_order_release);

    return receiveSize;
}

void VirtualRDMARingBuffer::waitUntilSendFree(size_t sizeToWrite) {
    // Make sure, there is enough space
    size_t safeToWrite = size - (sendPos - remoteReadPos.load());
    while (sizeToWrite > safeToWrite) {
        rdma::ReadWorkRequestBuilder(remoteReadPosMr, remoteReadPosRmr, true).send(net.queuePair);
        while (net.completionQueue.pollSendCompletionQueue() != rdma::ReadWorkRequest::getId());
        // Poll until read has finished
        safeToWrite = size - (sendPos - remoteReadPos.load());
    }
}
