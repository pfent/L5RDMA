#ifndef EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H
#define EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <exchangeableTransports/util/sharedMemory.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

struct QueueMessage {
    std::atomic<QueueMessage *> next;
    size_t size;
    uint8_t data[];
    static constexpr size_t MESSAGE_HEADER_SIZE = sizeof(next) + sizeof(size);
};

struct QueueSharedBuffer {
    std::atomic<size_t> freePos;
    uint8_t buffer[];
    static constexpr size_t BUFFER_HEADER_SIZE = sizeof(freePos);
};

#pragma GCC diagnostic pop

/**
 * A single producer, single consumer lock-free message queue, as proposed by intel (esp. for memory orders):
 * @link https://software.intel.com/en-us/articles/single-producer-single-consumer-queue
 */
struct SharedMemoryMessageQueue {
    const std::string bufferName = "sharedBuffer" + std::to_string(::getpid());
    const size_t size;

    QueueMessage *head;
    QueueMessage *tail;
    size_t endOfLastWrite = 0;

    std::shared_ptr<QueueSharedBuffer> local;
    const SharedMemoryInfo info; // first init local, then exchange info
    std::shared_ptr<QueueSharedBuffer> remote;

    /// Establish a shared memory region of size with the remote side of sock
    SharedMemoryMessageQueue(size_t size, int sock);

    void send(const uint8_t *data, size_t length);

    size_t receive(void *whereTo, size_t maxSize);

private:
    QueueMessage* getNewMessage(size_t dataSize);

    void releaseOld(QueueMessage *toRelease);
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEQUEUE_H
