#include "SharedMemoryMessageQueue.h"
#include <exchangeableTransports/util/virtualMemory.h>

using namespace std;

// TODO: this can't work, because we operate on separate virtual memory spaces, so we segfault when trying to access
// the other processes memory. Better create offsets here
QueueMessage *SharedMemoryMessageQueue::getNewMessage(size_t dataSize) {
    const size_t messageLength = dataSize + QueueMessage::MESSAGE_HEADER_SIZE;

    size_t bufferPos = endOfLastWrite % size;
    if (bufferPos + messageLength > size) { // wraparound
        bufferPos = 0;
    }
    // get remote usage of the buffer and calculate safeToWrite
    const size_t safeToWrite = [&]() {
        size_t freePos = remote->freePos;
        if (bufferPos > freePos) {
            return size - bufferPos;
        } else if (bufferPos < freePos) {
            return freePos - bufferPos;
        } else {
            return size;
        }
    }();
    if (safeToWrite < messageLength) {
        throw runtime_error{"can't fit message into message queue"};
    }

    auto newMessage = reinterpret_cast<QueueMessage *>(&local->buffer[bufferPos]);

    endOfLastWrite = bufferPos + messageLength;

    return newMessage;
}

void SharedMemoryMessageQueue::releaseOld(QueueMessage *toRelease) {
    size_t lastFreePos = local->freePos;
    lastFreePos += QueueMessage::MESSAGE_HEADER_SIZE + toRelease->size;
    local->freePos = lastFreePos % size;
}

void SharedMemoryMessageQueue::send(const uint8_t *data, size_t length) {
    QueueMessage *newMsg = getNewMessage(length);

    newMsg->next.store(nullptr, memory_order::memory_order_relaxed);
    newMsg->size = length;
    copy(data, &data[length], newMsg->data);

    // for multiple senders, this needs to happen atomically, but assume we have a single sender now
    head->next.store(newMsg, memory_order::memory_order_release);
    head = newMsg;
}

size_t SharedMemoryMessageQueue::receive(void *whereTo, size_t maxSize) {
    while (tail->next.load(memory_order::memory_order_consume) == nullptr) /* Busy wait for now */;
    QueueMessage *next = tail->next.load(memory_order::memory_order_relaxed);
    QueueMessage *old = tail;
    tail = next;

    const size_t ret = next->size;
    if (ret > maxSize) {
        throw runtime_error{"plz only read whole messages for now!"};
    }
    copy(next->data, &next->data[next->size], reinterpret_cast<uint8_t *>(whereTo));
    releaseOld(old);
    return ret;
}

SharedMemoryMessageQueue::SharedMemoryMessageQueue(size_t size, int sock) :
        size(size),
        local(malloc_shared<QueueSharedBuffer>(bufferName, QueueSharedBuffer::BUFFER_HEADER_SIZE + size, true)),
        info(sock, bufferName),
        remote(malloc_shared<QueueSharedBuffer>(info.remoteBufferName, QueueSharedBuffer::BUFFER_HEADER_SIZE + size,
                                                false)) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }
    head = reinterpret_cast<QueueMessage *>(&local->buffer[0]);
    tail = reinterpret_cast<QueueMessage *>(&remote->buffer[0]);
    endOfLastWrite = QueueMessage::MESSAGE_HEADER_SIZE; // start with one zero size sentinel in the beginning
}
