#include "VirtualRDMARingBuffer.h"
#include <exchangeableTransports/rdma/WorkRequest.hpp>
#include <sys/mman.h>
#include <fcntl.h>

using Permission = rdma::MemoryRegion::Permission;

WraparoundBuffer mmapRingBuffer(size_t size, const char *backingFile) {
    const auto fd = open(backingFile, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw std::runtime_error{"shm_open failed"};
    }
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw std::runtime_error{"ftruncate failed"};
    }
    auto first = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    // map the same file in virtual memory directly after the first one
    auto second = mmap(reinterpret_cast<uint8_t *>(first),
                       size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, 0);
    if (second == reinterpret_cast<void *>(-1)) {
        perror("mmap");
        throw std::runtime_error{"mmaping the wraparound failed"};
    }
    close(fd); // no need to keep the writeFd open after the mapping

    const auto deleter = [size](void *p) {
        munmap(p, size);
    };

    return {std::shared_ptr < uint8_t > (reinterpret_cast<uint8_t *>(first), deleter),
            std::shared_ptr < uint8_t > (reinterpret_cast<uint8_t *>(second), deleter)};
}

constexpr auto localRw = Permission::LocalWrite | Permission::RemoteWrite;

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, int sock) :
        size(size), bitmask(size - 1), net(sock),
        local(mmapRingBuffer(size, "/tmp/rdmaLocal")),
        // Since we mapped twice the virtual memory, we can create memory regions of twice the size of the actual buffer
        localSendMr(this->local.get(), size * 2, net.network.getProtectionDomain(), Permission::None),
        localReadPosMr(&localReadPos, sizeof(localReadPos), net.network.getProtectionDomain(), Permission::RemoteRead),
        remote(mmapRingBuffer(size, "/tmp/rdmaRemote")),
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

    // TODO: check if it's safe to write (compare RDMAMessageBuffer::writeToSendBuffer)

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
    const auto sizeToRead = sizeof(maxSize) + maxSize + sizeof(validity);
    if (sizeToRead > size) throw std::runtime_error{"receiveSize > buffersize!"};

    const auto lastReadPos = localReadPos.load();
    const auto startOfRead = lastReadPos & bitmask;
    const auto readFromBuffer = [&](auto fromOffset, auto dest, auto howManyBytes) {
        std::copy(&local.get()[fromOffset], &local.get()[fromOffset + howManyBytes], reinterpret_cast<uint8_t *>(dest));
    };

    // TODO: check messageLength != 0
    std::atomic<size_t> length;
    std::atomic<size_t> checkMe;
    do {
        readFromBuffer(startOfRead, &length, sizeof(length));
        readFromBuffer(startOfRead + sizeof(length) + length, &checkMe, sizeof(checkMe));
    } while (checkMe.load() != validity);
    // TODO probably need a second readFromBuffer of length, so we didn't by chance read the validity
    const auto finalLength = length.load();
    // TODO: check length == maxSize

    readFromBuffer(startOfRead + sizeof(length), whereTo, finalLength);
    // TODO: zero receiveBuffer

    localReadPos.store(lastReadPos + sizeToRead, std::memory_order_release);

    return finalLength;
}
