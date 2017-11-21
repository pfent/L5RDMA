#ifndef EXCHANGABLETRANSPORTS_BUFFER_H
#define EXCHANGABLETRANSPORTS_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>

struct Buffer {
    size_t size;
    uint8_t *ptr;

    Buffer(size_t size, uint8_t *ptr) : size(size), ptr(ptr) {}

    // move-only type. Don't destruct it without markAsSent
    Buffer(Buffer &&) = default;

    Buffer &operator=(Buffer &&) = default;

    Buffer(const Buffer &) = delete;

    Buffer &operator=(const Buffer &) = delete;

    ~Buffer() {
        if (ptr != nullptr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"
            throw std::runtime_error{"please don't leak buffers"};
#pragma GCC diagnostic pop
        }
    }

    void markAsSent() {
        ptr = nullptr;
    }
};

#endif //EXCHANGABLETRANSPORTS_BUFFER_H
