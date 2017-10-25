#ifndef RDMA_SOCKETS_SHAREDMEMORYMESSAGEBUFFER_H
#define RDMA_SOCKETS_SHAREDMEMORYMESSAGEBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>

struct Message {
    size_t size;
    Message *next;
};

struct SharedMemoryMessageBuffer {
    size_t totalSize;
    std::atomic<Message *> incoming = nullptr;
    std::atomic<Message *> outgoing = nullptr;
    static constexpr auto HEADER_SIZE = sizeof(totalSize);

    uint8_t inBuffer[1024];
    uint8_t outBuffer[1024];

    /**
     * Todo
     */

};

#endif //RDMA_SOCKETS_SHAREDMEMORYMESSAGEBUFFER_H
