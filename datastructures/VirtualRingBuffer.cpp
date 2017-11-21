#include "VirtualRingBuffer.h"
#include <exchangeableTransports/util/sharedMemory.h>

VirtualRingBuffer::VirtualRingBuffer(size_t size, int sock) : size(size), bitmask(size - 1) {
    localRw = malloc_shared<RingBufferInfo>(infoName + pid, sizeof(RingBufferInfo), true);
    local1 = malloc_shared<uint8_t>(bufferName + pid, size, true);
    local2 = malloc_shared<uint8_t>(bufferName + pid, size, false, &local1.get()[size]);

    domain_write(sock, pid.c_str(), pid.size());
    uint8_t buffer[255];
    size_t readCount = domain_read(sock, buffer, 255);
    const auto remotePid = std::string(buffer, buffer + readCount);

    remoteRw = malloc_shared<RingBufferInfo>(infoName + remotePid, sizeof(RingBufferInfo), false);
    remote1 = malloc_shared<uint8_t>(bufferName + remotePid, size, false);
    remote2 = malloc_shared<uint8_t>(bufferName + remotePid, size, false, &remote1.get()[size]);
}

void VirtualRingBuffer::send(const uint8_t *data, size_t length) {
    const auto localWritten = localRw->written.load();
    const auto pos = localWritten & bitmask;

    size_t remoteRead;
    do {
        remoteRead = remoteRw->read; // probably buffer this in class, so we don't have as much remote reads
    } while ((localWritten - remoteRead) > (size - length)); // block until there is some space

    std::copy(data, data + length, &local1.get()[pos]);

    // basically `localRw->written += length;`, but without the mfence or locked instructions
    localRw->written.store(localWritten + length, std::memory_order_release);
}

size_t VirtualRingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto localRead = localRw->read.load();
    const auto pos = localRead & bitmask;

    size_t remoteWritten;
    do {
        remoteWritten = remoteRw->written; // probably buffer this in class, so we don't have as much remote reads
    } while ((remoteWritten - localRead) < maxSize); // block until maxSize is available

    std::copy(&remote1.get()[pos], &remote1.get()[pos + maxSize], reinterpret_cast<uint8_t *>(whereTo));

    // basically `localRw->read += maxSize;`, but without the mfence or locked instructions
    localRw->read.store(localRead + maxSize, std::memory_order_release);
    return maxSize;
}

Buffer VirtualRingBuffer::reserveBuffer(size_t size) {
    return Buffer(0, nullptr);
}


