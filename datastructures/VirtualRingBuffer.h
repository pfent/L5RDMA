#ifndef EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H
#define EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <exchangeableTransports/transports/Buffer.h>

struct RingBufferInfo {
    std::atomic<size_t> read;
    std::atomic<size_t> written;
};

/// http://ourmachinery.com/post/virtual-memory-tricks/
struct VirtualRingBuffer {
    const std::string bufferName = "sharedBuffer";
    const std::string infoName = "sharedRw";
    const std::string pid = std::to_string(::getpid());
    const size_t size;
    const size_t bitmask;

    std::shared_ptr<RingBufferInfo> localRw;
    std::shared_ptr<uint8_t> local1;
    std::shared_ptr<uint8_t> local2;

    std::shared_ptr<RingBufferInfo> remoteRw;
    std::shared_ptr<uint8_t> remote1;
    std::shared_ptr<uint8_t> remote2;

    /// Establish a shared memory region of size with the remote side of sock
    VirtualRingBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);

    Buffer reserveBufferForSending(size_t length);

    void send(Buffer buffer);

    Buffer receiveIntoBuffer(size_t length);

    void markAsRead(Buffer buffer);

private:
    void waitUntilSendFree(size_t localWritten, size_t length) const;

    void waitUntilReceiveAvailable(size_t maxSize, size_t localRead) const;
};

#endif //EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H
