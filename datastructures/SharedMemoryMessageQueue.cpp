#include "SharedMemoryMessageQueue.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstddef>
#include <exchangeableTransports/util/domainSocketsWrapper.h>
#include <cstring>
#include <xmmintrin.h>

using namespace std;

template<typename T>
std::shared_ptr<T> malloc_shared(const string &name, size_t size, bool init) {
    // create a new mapping in /dev/shm
    const auto fd = shm_open(name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw runtime_error{"shm_open failed"};
    }
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw runtime_error{"ftruncate failed"};
    }

    auto deleter = [size](void *p) {
        munmap(p, size);
    };
    auto ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd); // no need to keep the fd open after the mapping

    if (init) {
        memset(ptr, 0, size);
    }

    return shared_ptr < T > (reinterpret_cast<T *>(ptr), deleter);
}

SharedMemoryInfo::SharedMemoryInfo(int sock, const std::string &bufferName) {
    domain_write(sock, bufferName.c_str(), bufferName.size());
    uint8_t buffer[255];
    size_t readCount = domain_read(sock, buffer, 255);
    this->remoteBufferName = string(buffer, buffer + readCount);
}

Message *getNewMessage(size_t dataSize); // TODO
void releaseNext(Message *);

void SharedMemoryMessageQueue::send(const uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(std::atomic<Message *>) + sizeof(size_t) + length;
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};
    // TODO: safeToWrite

    Message *newMsg = getNewMessage(length);
    newMsg->next.store(nullptr, memory_order::memory_order_relaxed);
    copy(data, &data[length], newMsg->data);

    // for multiple senders, this needs to happen atomically, but assume we have a single sender now
    head->next.store(newMsg, memory_order::memory_order_release);
    head = newMsg;
}

size_t SharedMemoryMessageQueue::receive(void *whereTo, size_t maxSize) {
    while (tail->next.load(memory_order::memory_order_consume) == nullptr) /* Busy wait for now */;
    Message *next = tail->next.load(memory_order::memory_order_relaxed);
    tail = next;

    copy(next->data, &next->data[next->size], reinterpret_cast<uint8_t *>(whereTo));
    const size_t ret = next->size;
    releaseNext(next);
    return ret;
}

SharedMemoryMessageQueue::SharedMemoryMessageQueue(size_t size, int sock) : // TODO: init last / end
        size(size),
        local(malloc_shared<SharedBuffer>(bufferName, sizeof(std::atomic<size_t>) + size, true)),
        info(sock, bufferName),
        remote(malloc_shared<SharedBuffer>(info.remoteBufferName, sizeof(std::atomic<size_t>) + size, false)) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }
    // TODO
}
