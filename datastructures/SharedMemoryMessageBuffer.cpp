#include "SharedMemoryMessageBuffer.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstddef>
#include <exchangeableTransports/util/domainSocketsWrapper.h>

using namespace std;

template<typename T>
std::shared_ptr<T> malloc_shared(const string &name, size_t size) {
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

    return shared_ptr < T > (reinterpret_cast<T *>(ptr), deleter);
}

SharedMemoryInfo::SharedMemoryInfo(int sock, const std::string &bufferName) {
    domain_write(sock, bufferName.c_str(), bufferName.size());
    uint8_t buffer[255];
    size_t readCount = domain_read(sock, buffer, 255);
    this->remoteBufferName = string(buffer, buffer + readCount);
}

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
    size_t recvSize;
    do {
        recvSize = message->size;
    } while (recvSize == 0) /*Busy wait for now, probably should be more clever here*/;

    if (recvSize > maxSize) {
        throw runtime_error{"plz only read whole messages for now!"};
    }

    // TODO: wraparound?
    copy(message->data, &message->data[recvSize], reinterpret_cast<uint8_t *>(whereTo));
    local->readPos += recvSize;

    return recvSize;
}

SharedMemoryMessageBuffer::SharedMemoryMessageBuffer(size_t size, int sock) :
        size(size),
        info(sock, bufferName),
        sendPos(0),
        local(malloc_shared<SharedBuffer>(bufferName, sizeof(std::atomic<size_t>) + size)),
        remote(malloc_shared<SharedBuffer>(info.remoteBufferName, sizeof(std::atomic<size_t>) + size)) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }
    // TODO
}
