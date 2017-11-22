#ifndef EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
#define EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <exchangeableTransports/transports/Buffer.h>

class VirtualRDMARingBuffer {
    const size_t size;
    const size_t bitmask;

    //std::shared_ptr<RingBufferInfo> localRw;
    std::shared_ptr<uint8_t> local1;
    std::shared_ptr<uint8_t> local2; // safeguarding virtual memory region, using the MMU for wraparound

    //std::shared_ptr<RingBufferInfo> remoteRw;
    std::shared_ptr<uint8_t> remote1;
    std::shared_ptr<uint8_t> remote2; // safeguarding virtual memory region, using the MMU for wraparound

    /// Establish a shared memory region of size with the remote side of sock
    VirtualRDMARingBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);

    Buffer reserveBufferForSending(size_t length);

    void send(Buffer buffer);

    Buffer receiveIntoBuffer(size_t length);

    void markAsRead(Buffer buffer);
};

#endif //EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
