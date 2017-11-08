#include "SharedMemoryMessageBuffer.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstddef>

using namespace std;

auto malloc_shared(string name, size_t size) {
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

    unique_ptr<uint8_t *, decltype(deleter)> uptr(reinterpret_cast<uint8_t **>(ptr), deleter);

    return uptr;
}


void SharedMemoryMessageBuffer::send(const uint8_t *data, size_t length) {
    // TODO
}

size_t SharedMemoryMessageBuffer::receive(void *whereTo, size_t maxSize) {
    return 0; // TODO
}

SharedMemoryMessageBuffer::SharedMemoryMessageBuffer(size_t size, int sock) :
        size(size) {
    auto ptr = malloc_shared("test"s, 1024);
    // TODO
}
