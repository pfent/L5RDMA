#include "SharedMemoryMessageBuffer.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstddef>
#include <exchangeableTransports/util/domainSocketsWrapper.h>

using namespace std;

template<typename T>
shared_ptr<T> malloc_shared(const string &name, size_t size) {
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

SharedMemoryInfo::SharedMemoryInfo(int sock, const std::string &bufferName, const std::string &readPosName) {
    domain_write(sock, bufferName.c_str(), bufferName.size());
    domain_write(sock, readPosName.c_str(), readPosName.size());
    uint8_t buffer[255];
    size_t readCount = domain_read(sock, buffer, 255);
    this->remoteBufferName = string(buffer, buffer + readCount);
    readCount = domain_read(sock, buffer, 255);
    this->remoteReadPosName = string(buffer, buffer + readCount);
}

void SharedMemoryMessageBuffer::send(const uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(length) + length;
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};
    // TODO: safeToWrite && wraparound
    const size_t startOfWrite = sendPos;
    sendPos += sizeToWrite;

    auto message = reinterpret_cast<Message *>(&*localSendBuffer[startOfWrite]);
    copy(data, &data[length], message->data);
    message->size.store(length, memory_order_release);
}

size_t SharedMemoryMessageBuffer::receive(void *whereTo, size_t maxSize) {
    auto message = reinterpret_cast<Message *>(&*remoteSendBuffer[readPos]);
    size_t recvSize;
    do {
        recvSize = message->size;
    } while (recvSize == 0) /*Busy wait for now, probably should be more clever here*/;

    if (recvSize > maxSize) {
        throw runtime_error{"plz only read whole messages for now!"};
    }

    // TODO: update readpos, maybe compare&swap? also wraparound?

    copy(message->data, &message->data[recvSize], reinterpret_cast<uint8_t *>(whereTo));
    return recvSize;
}

SharedMemoryMessageBuffer::SharedMemoryMessageBuffer(size_t size, int sock)
        : // TODO: some kind of ID to differentiate client / server
        size(size),
        info(sock, "sendBuffer", "readPos"),
        localSendBuffer(malloc_shared<uint8_t *>("sendBuffer", size)),
        remoteReadPos(malloc_shared<std::atomic<size_t>>(info.remoteReadPosName, sizeof(std::atomic<size_t>))),
        sendPos(0),
        remoteSendBuffer(malloc_shared<uint8_t *>(info.remoteBufferName, size)),
        readPos(malloc_shared<std::atomic<size_t>>("readPos", sizeof(std::atomic<size_t>))) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }
    // TODO
}
