#include "SharedMemoryMessageBuffer.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstddef>

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


void SharedMemoryMessageBuffer::send(const uint8_t *data, size_t length) {
    // TODO
}

size_t SharedMemoryMessageBuffer::receive(void *whereTo, size_t maxSize) {
    return 0; // TODO
}

SharedMemoryMessageBuffer::SharedMemoryMessageBuffer(size_t size, int sock) :
        size(size) {
    localSend = malloc_shared<uint8_t *>("test", size);
    localReceive = malloc_shared<uint8_t *>("test2", size);
    readPos = malloc_shared<std::atomic<size_t>>("test3", sizeof(std::atomic<size_t>));
    // TODO
}
