#ifndef EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
#define EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>

struct Message {
    std::atomic<size_t> size;
    uint8_t data[];
};

struct SharedBuffer {
    std::atomic<size_t> readPos;
    uint8_t buffer[];
};

struct SharedMemoryInfo {
    std::string remoteBufferName;

    /// Exchange shared memory information with the remote side of the socket
    SharedMemoryInfo(int sock, const std::string &bufferName);
};

struct SharedMemoryMessageBuffer {
    const size_t size;
    const SharedMemoryInfo info;
    const std::string bufferName = "sharedBuffer" + std::to_string(::getpid());

    size_t sendPos;
    std::shared_ptr<SharedBuffer> local;
    std::shared_ptr<SharedBuffer> remote;

    /// Establish a shared memory region of size with the remote side of sock
    SharedMemoryMessageBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
