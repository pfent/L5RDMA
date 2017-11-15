#include "SharedMemoryMessageBuffer.h"

#include <exchangeableTransports/util/domainSocketsWrapper.h>
#include <exchangeableTransports/util/sharedMemory.h>

using namespace std;

void SharedMemoryMessageBuffer::send(const uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(length) + length;
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};
    // TODO: safeToWrite && wraparound
    const size_t startOfWrite = sendPos;
    sendPos += sizeToWrite;

    auto message = reinterpret_cast<Message *>(&local->buffer[startOfWrite]);
    copy(data, &data[length], message->data);
    message->size.store(length, memory_order_release);
}

size_t SharedMemoryMessageBuffer::receive(void *whereTo, size_t maxSize) {
    const auto currentPos = local->readPos.load();
    auto message = reinterpret_cast<Message *>(&remote->buffer[currentPos]);
    size_t recvSize = message->size;

    if (recvSize == 0) { // TODO probably be more clever here
        sched_yield(); // _mm_pause(); or nanosleep(); depending on a sampling of the average wait times
    }

    while (recvSize == 0) {
        recvSize = message->size;
    }

    if (recvSize > maxSize) {
        throw runtime_error{"plz only read whole messages for now!"};
    }

    // TODO: wraparound?
    copy(message->data, &message->data[recvSize], reinterpret_cast<uint8_t *>(whereTo));

    const auto messageSize = sizeof(message->size) + recvSize;
    local->readPos += messageSize;

    return recvSize;
}

SharedMemoryMessageBuffer::SharedMemoryMessageBuffer(size_t size, int sock) :
        size(size),
        sendPos(0),
        local(malloc_shared<SharedBuffer>(bufferName, sizeof(std::atomic<size_t>) + size, true)),
        info(sock, bufferName),
        remote(malloc_shared<SharedBuffer>(info.remoteBufferName, sizeof(std::atomic<size_t>) + size, false)) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }
    // TODO
}
