#include "VirtualRingBuffer.h"
#include <sys/mman.h>
#include "util/virtualMemory.h"
#include "util/busywait.h"

VirtualRingBuffer::VirtualRingBuffer(size_t size, int sock) : size(size), bitmask(size - 1) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    localRw = malloc_shared<RingBufferInfo>(infoName + pid, sizeof(RingBufferInfo), true);
    local = mmapSharedRingBuffer(bufferName + pid, size, true);

    domain_write(sock, pid.c_str(), pid.size());
    uint8_t buffer[255];
    size_t readCount = domain_read(sock, buffer, 255);
    const auto remotePid = std::string(buffer, buffer + readCount);

    remoteRw = malloc_shared<RingBufferInfo>(infoName + remotePid, sizeof(RingBufferInfo), false);
    remote = mmapSharedRingBuffer(bufferName + remotePid, size);
}

void VirtualRingBuffer::waitUntilSendFree(size_t localWritten, size_t length) const {
    size_t remoteRead;
    loop_while([&]() {
        remoteRead = remoteRw->read; // probably buffer this in class, so we don't have as much remote reads
    }, [&]() { return (localWritten - remoteRead) > (size - length); }); // block until there is some space
}

void VirtualRingBuffer::send(const uint8_t *data, size_t length) {
    const auto localWritten = localRw->written.load();
    const auto pos = localWritten & bitmask;

    waitUntilSendFree(localWritten, length);

    std::copy(data, data + length, &local.get()[pos]);

    // basically `localRw->written += length;`, but without the mfence or locked instructions
    localRw->written.store(localWritten + length, std::memory_order_release);
}

size_t VirtualRingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto localRead = localRw->read.load();
    const auto pos = localRead & bitmask;

    waitUntilReceiveAvailable(maxSize, localRead);

    std::copy(&remote.get()[pos], &remote.get()[pos + maxSize], reinterpret_cast<uint8_t *>(whereTo));

    // basically `localRw->read += maxSize;`, but without the mfence or locked instructions
    localRw->read.store(localRead + maxSize, std::memory_order_release);
    return maxSize;
}

void VirtualRingBuffer::waitUntilReceiveAvailable(size_t maxSize, size_t localRead) const {
    size_t remoteWritten;
    loop_while([&]() {
        remoteWritten = remoteRw->written; // probably buffer this in class, so we don't have as much remote reads
    }, [&]() { return (remoteWritten - localRead) < maxSize; }); // block until maxSize is available
}

Buffer VirtualRingBuffer::reserveBufferForSending(size_t length) {
    const auto localWritten = localRw->written.load();
    const auto pos = localWritten & bitmask;

    waitUntilSendFree(localWritten, length);

    return Buffer(length, &local.get()[pos]);
}

void VirtualRingBuffer::send(Buffer buffer) {
    localRw->written.store(localRw->written + buffer.size, std::memory_order_release);
    buffer.markAsDone();
}

Buffer VirtualRingBuffer::receiveIntoBuffer(size_t length) {
    const auto localRead = localRw->read.load();
    const auto pos = localRead & bitmask;

    waitUntilReceiveAvailable(length, localRead);

    return Buffer(length, &remote.get()[pos]);
}

void VirtualRingBuffer::markAsRead(Buffer buffer) {
    localRw->read.store(localRw->read + buffer.size,
                        std::memory_order_release); // Probably can shave off this atomic read
    buffer.markAsDone();
}


