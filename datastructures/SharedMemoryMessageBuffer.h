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

// maybe do as comparison: http://people.cs.pitt.edu/~jacklange/teaching/cs2510-f12/papers/implementing_lock_free.pdf
struct SharedMemoryMessageBuffer {
    const size_t size;

    std::unique_ptr<uint8_t[]> localSend;
    std::unique_ptr<uint8_t[]> localReceive;

    std::unique_ptr<std::atomic<size_t>> readPos;
    size_t sendPos;

    uint8_t *remoteSend;
    uint8_t *remoteReceive;

    SharedMemoryMessageBuffer(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
