#ifndef EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
#define EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <memory>

struct Message {
    std::atomic<size_t> size;
    uint8_t data[];
};

struct SharedMemoryInfo {
    std::string remoteBufferName;
    std::string remoteReadPosName;

    /// Exchange shared memory information with the remote side of the socket
    SharedMemoryInfo(int sock, const std::string &bufferName, const std::string &readPosName);
};

struct SharedMemoryMessageBuffer {
    const size_t size;
    const SharedMemoryInfo info;

    std::shared_ptr<uint8_t *> localSendBuffer;
    std::shared_ptr<std::atomic<size_t>> remoteReadPos;
    size_t sendPos;

    std::shared_ptr<uint8_t *> remoteSendBuffer;
    std::shared_ptr<std::atomic<size_t>> readPos;

    /// Establish a shared memory region of size with the remote side of sock
    SharedMemoryMessageBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
