#ifndef EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H
#define EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H

#include <atomic>
#include <memory>
#include "util/virtualMemory.h"

namespace l5 {
namespace util {
class Socket;
}
namespace datastructure {
struct RingBufferInfo {
    std::atomic<size_t> read;
    std::atomic<size_t> written;
};

/// http://ourmachinery.com/post/virtual-memory-tricks/
struct VirtualRingBuffer {
    const std::string bufferName = "/sharedBuffer";
    const std::string infoName = "/sharedRw";
    const std::string pid = std::to_string(::getpid());
    const size_t size;
    const size_t bitmask;

    util::ShmMapping<RingBufferInfo> localRw;
    util::WraparoundBuffer local;

    size_t cachedRemoteRead;
    util::ShmMapping<RingBufferInfo> remoteRw;
    util::WraparoundBuffer remote;

    /// Establish a shared memory region of size with the remote side of sock
    VirtualRingBuffer(size_t size, const util::Socket &sock);

    void send(const uint8_t *data, size_t length);

    /// Receive exactly size bytes
    size_t receive(void *whereTo, size_t maxSize);

    /// Receive at least 1, up to maxSize bytes
    size_t receiveSome(void* whereTo, size_t maxSize);

private:
    void waitUntilSendFree(size_t localWritten, size_t length);

    void waitUntilReceiveAvailable(size_t maxSize, size_t localRead);
};
} // namespace datastructure
} // namespace l5

#endif //EXCHANGABLE_TRANSPORTS_VIRTUALRINGBUFFER_H
