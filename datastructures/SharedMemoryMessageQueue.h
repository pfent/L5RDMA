#ifndef EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H
#define EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

struct Message {
    std::atomic<Message *> next;
    size_t size;
    uint8_t data[];
};

using SharedBuffer = uint8_t *;

#pragma GCC diagnostic pop

struct SharedMemoryInfo {
    std::string remoteBufferName;

    /// Exchange shared memory information with the remote side of the socket
    SharedMemoryInfo(int sock, const std::string &bufferName);
};

struct SharedMemoryMessageQueue {
    const std::string bufferName = "sharedBuffer" + std::to_string(::getpid());
    const size_t size;

    std::atomic<Message *> last; // TODO: init this pointers to the very first buffer position with next = nullptr, size = 0
    std::atomic<Message *> end;

    std::shared_ptr<SharedBuffer> local;
    const SharedMemoryInfo info; // first init local, then exchange info
    std::shared_ptr<SharedBuffer> remote;

    /// Establish a shared memory region of size with the remote side of sock
    SharedMemoryMessageQueue(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H
