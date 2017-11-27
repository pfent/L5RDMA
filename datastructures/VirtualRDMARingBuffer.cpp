#include "VirtualRDMARingBuffer.h"
#include <exchangeableTransports/rdma/WorkRequest.hpp>
#include <sys/mman.h>
#include <fcntl.h>

using Permission = rdma::MemoryRegion::Permission;

constexpr auto localRw = Permission::LocalWrite | Permission::RemoteWrite;

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, int sock) :
        size(size), bitmask(size - 1), net(sock),
        local(mmapRDMARingBuffer("/tmp/rdmaLocal", size, true)),
        // Since we mapped twice the virtual memory, we can create memory regions of twice the size of the actual buffer
        localSendMr(this->local.get(), size * 2, net.network.getProtectionDomain(), Permission::None),
        localReadPosMr(&localReadPos, sizeof(localReadPos), net.network.getProtectionDomain(), Permission::RemoteRead),
        remote(mmapRDMARingBuffer("/tmp/rdmaRemote", size, true)),
        localReceiveMr(this->remote.get(), size * 2, net.network.getProtectionDomain(), localRw),
        remoteReadPosMr(&remoteReadPos, sizeof(remoteReadPos), net.network.getProtectionDomain(),
                        Permission::RemoteRead) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    sendRmrInfo(sock, localReadPosMr, remoteReadPosMr);
    receiveAndSetupRmr(sock, remoteReceiveRmr, remoteReadPosRmr);
}

void VirtualRDMARingBuffer::send(const uint8_t *data, size_t length) {
    const auto sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw std::runtime_error{"data > buffersize!"};

    const auto startOfWrite = sendPos & bitmask;
    auto whereToWrite = startOfWrite;
    const auto write = [&](auto what, auto howManyBytes) {
        std::copy(reinterpret_cast<const uint8_t *>(what), &reinterpret_cast<const uint8_t *>(what)[howManyBytes],
                  &local.get()[whereToWrite]);
        whereToWrite += howManyBytes;
    };

    waitUntilSendFree(sizeToWrite);

    // first write the data
    write(&length, sizeof(length));
    write(data, length);
    write(&validity, sizeof(validity));

    // then request it to be sent via RDMA
    const auto sendSlice = localSendMr.slice(startOfWrite, sizeToWrite);
    const auto remoteSlice = remoteReadPosRmr.slice(startOfWrite);
    rdma::WriteWorkRequestBuilder(sendSlice, remoteSlice, false)
            .setInline(sendSlice.size <= net.queuePair.getMaxInlineSize())
            .send(net.queuePair);

    // finally, update sendPos
    sendPos += sizeToWrite;
}

size_t VirtualRDMARingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto maxSizeToRead = sizeof(maxSize) + maxSize + sizeof(validity);
    if (maxSizeToRead > size) throw std::runtime_error{"receiveSize > buffersize!"};

    const auto lastReadPos = localReadPos.load();
    const auto startOfRead = lastReadPos & bitmask;
    const auto readFromBuffer = [&](auto fromOffset, auto dest, auto howManyBytes) {
        std::copy(&local.get()[fromOffset], &local.get()[fromOffset + howManyBytes], reinterpret_cast<uint8_t *>(dest));
    };

    std::atomic<size_t> receiveSize;
    std::atomic<size_t> checkMe;
    do {
        readFromBuffer(startOfRead, &receiveSize, sizeof(receiveSize));
        readFromBuffer(startOfRead + sizeof(receiveSize) + receiveSize.load(), &checkMe, sizeof(checkMe));
    } while (checkMe.load() != validity);
    // TODO probably need a second readFromBuffer of length, so we didn't by chance read the validity
    const auto finalLength = receiveSize.load();

    if (finalLength > maxSize) {
        throw std::runtime_error{"plz only read whole messages for now!"}; // probably buffer partially read msgs
    }

    readFromBuffer(startOfRead + sizeof(receiveSize), whereTo, finalLength);

    const auto sizeToRead = sizeof(maxSize) + finalLength + sizeof(validity);
    std::fill(&local.get()[startOfRead], &local.get()[startOfRead + sizeToRead], 0);

    localReadPos.store(lastReadPos + sizeToRead, std::memory_order_release);

    return finalLength;
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
