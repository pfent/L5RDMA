#ifndef EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
#define EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <algorithm>
#include <stdexcept>

struct Message {
    std::atomic<Message *> next;
    size_t dataLength;
    uint8_t data[];

    static constexpr size_t getTotalLength(size_t length) {
        return length + sizeof(next) + sizeof(dataLength);
    }
};

struct SharedMemoryMessageBuffer {
    size_t totalSize;
    std::atomic<size_t> current = 0;
    std::atomic<Message *> head = nullptr;
    std::atomic<Message *> tail = nullptr;
    static constexpr auto HEADER_SIZE = sizeof(totalSize);

    uint8_t inBuffer[10 * 1024 * 1024];
    uint8_t outBuffer[10 * 1024 * 1024];

    // TODO

    void send(const uint8_t *data, size_t length) {
    }

    size_t receive(void *whereTo, size_t maxSize) {
        return 0;
    }

    // TODO: this still needs to ensure, we don't run out of space. I.e. wraparound

    // from http://people.cs.pitt.edu/~jacklange/teaching/cs2510-f12/papers/implementing_lock_free.pdf
    void enqueue(const uint8_t *data, size_t length) {
        const auto whereToWrite = current.fetch_add(Message::getTotalLength(length));
        auto thisMessagePtr = reinterpret_cast<Message *>(outBuffer + whereToWrite); // TODO: wraparound
        Message *expected = nullptr;

        std::copy(data, data + length, reinterpret_cast<uint8_t *>(&thisMessagePtr->data));
        thisMessagePtr->dataLength = length;
        thisMessagePtr->next = nullptr;

        bool successful;
        Message *oldTail;
        do {
            oldTail = tail.load();
            successful = oldTail->next.compare_exchange_strong(expected, thisMessagePtr);
            if (not successful) {
                tail.compare_exchange_strong(oldTail, oldTail->next);
            }
        } while (not successful);
        tail.compare_exchange_strong(oldTail, thisMessagePtr);
    }

    size_t dequeue(void *whereTo, size_t size) {
        Message *oldHead;
        do {
            oldHead = head.load();
            if (oldHead->next == nullptr) {
                throw std::runtime_error{"queue empty, should we block?"};
            }
        } while (not head.compare_exchange_strong(oldHead, oldHead->next));

        if (oldHead->dataLength > size) {
            throw std::runtime_error{"read of invalid size. this probably isn't what you wanted"};
        }
        std::copy(oldHead->data, &oldHead->data[oldHead->dataLength], reinterpret_cast<uint8_t *>(whereTo));
        return oldHead->dataLength;
    }
};

#endif //EXCHANGABLE_TRANSPORTS_SHAREDMEMORYMESSAGEBUFFER_H
