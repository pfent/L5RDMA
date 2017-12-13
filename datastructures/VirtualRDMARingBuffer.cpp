#include "VirtualRDMARingBuffer.h"
#include <exchangeableTransports/rdma/WorkRequest.hpp>
#include <iostream>

using Perm = rdma::MemoryRegion::Permission;

constexpr auto localRw = Perm::LocalWrite | Perm::RemoteWrite;

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, int sock) :
        size(size), bitmask(size - 1), net(sock),
        sendBuf(mmapSharedRingBuffer("/rdmaLocal" + std::to_string(::getpid()), size, true)),
        // Since we mapped twice the virtual memory, we can create memory regions of twice the size of the actual buffer
        localSendMr(sendBuf.get(), size * 2, net.network.getProtectionDomain(),
                    Perm::All), // TODO: All permissions is probably too much
        localReadPosMr(&localReadPos, sizeof(localReadPos), net.network.getProtectionDomain(), Perm::All),
        receiveBuf(mmapSharedRingBuffer("/rdmaRemote" + std::to_string(::getpid()), size, true)),
        localReceiveMr(receiveBuf.get(), size * 2, net.network.getProtectionDomain(), Perm::All),
        remoteReadPosMr(&remoteReadPos, sizeof(remoteReadPos), net.network.getProtectionDomain(), Perm::All) {
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
    rdma::WriteWorkRequestBuilder(sendSlice, remoteSlice, true)
            .setInline(sendSlice.size <= net.queuePair.getMaxInlineSize())
            .send(net.queuePair);

    net.queuePair.getCompletionQueuePair().waitForCompletion();

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
        readFromBuffer(startOfRead, &receiveSize, sizeof(receiveSize));
        readFromBuffer(startOfRead + sizeof(receiveSize) + receiveSize, &checkMe, sizeof(checkMe));
    } while (checkMe != validity);
    // TODO probably need a second readFromBuffer of length, so we didn't by chance read the validity

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
