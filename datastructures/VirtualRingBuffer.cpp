#include "VirtualRingBuffer.h"
#include <sys/mman.h>
#include "util/virtualMemory.h"
#include "util/busywait.h"

namespace l5 {
namespace datastructure {
using namespace util;

VirtualRingBuffer::VirtualRingBuffer(size_t size, const Socket &sock) : size(size), bitmask(size - 1) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    localRw = malloc_shared<RingBufferInfo>(infoName + pid, sizeof(RingBufferInfo));
    domain::send_fd(sock, localRw.fd);

    local = mmapSharedRingBuffer(bufferName + pid, size, true);
    domain::send_fd(sock, local.fd);

    auto remoteRwFd = Socket::fromRaw(domain::receive_fd(sock));
    remoteRw = malloc_shared<RingBufferInfo>(remoteRwFd.get(), sizeof(RingBufferInfo));

    auto remoteFd = Socket::fromRaw(domain::receive_fd(sock));
    remote = mmapRingBuffer(remoteFd.get(), size);
}

void VirtualRingBuffer::waitUntilSendFree(size_t localWritten, size_t length) const {
    size_t remoteRead;
    loop_while([&]() {
        remoteRead = remoteRw.data->read; // probably buffer this in class, so we don't have as much remote reads
    }, [&]() { return (localWritten - remoteRead) > (size - length); }); // block until there is some space
}

void VirtualRingBuffer::send(const uint8_t *data, size_t length) {
    const auto localWritten = localRw.data->written.load();
    const auto pos = localWritten & bitmask;

    waitUntilSendFree(localWritten, length);

    std::copy(data, data + length, &local.data.get()[pos]);

    // basically `localRw->written += length;`, but without the mfence or locked instructions
    localRw.data->written.store(localWritten + length, std::memory_order_release);
}

size_t VirtualRingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto localRead = localRw.data->read.load();
    const auto pos = localRead & bitmask;

    waitUntilReceiveAvailable(maxSize, localRead);

    std::copy(&remote.data.get()[pos], &remote.data.get()[pos + maxSize], reinterpret_cast<uint8_t *>(whereTo));

    // basically `localRw->read += maxSize;`, but without the mfence or locked instructions
    localRw.data->read.store(localRead + maxSize, std::memory_order_release);
    return maxSize;
}

void VirtualRingBuffer::waitUntilReceiveAvailable(size_t maxSize, size_t localRead) const {
    size_t remoteWritten;
    loop_while([&]() {
        remoteWritten = remoteRw.data->written; // probably buffer this in class, so we don't have as much remote reads
    }, [&]() { return (remoteWritten - localRead) < maxSize; }); // block until maxSize is available
}
} // namespace datastructure
} // namespace l5
