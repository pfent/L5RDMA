#include "VirtualRDMARingBuffer.h"
#include <sys/mman.h>
#include <fcntl.h>

void mmapRingBuffer(size_t size, const char *backingFile,
                    std::shared_ptr<uint8_t> &main,
                    std::shared_ptr<uint8_t> &wraparound) {
    const auto name = "/tmp/rdmaLocal";
    const auto fd = open(name, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw std::runtime_error{"shm_open failed"};
    }
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw std::runtime_error{"ftruncate failed"};
    }
    auto first = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    // map the same file in virtual memory directly after the first one
    auto second = mmap(reinterpret_cast<uint8_t *>(first),
                       size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, 0);
    if (second == (void *) -1) {
        perror("mmap");
        throw std::runtime_error{"mmaping the wraparound failed"};
    }
    close(fd); // no need to keep the writeFd open after the mapping

    auto deleter = [size](void *p) {
        munmap(p, size);
    };

    main = std::shared_ptr < uint8_t > (reinterpret_cast<uint8_t *>(first), deleter);
    wraparound = std::shared_ptr < uint8_t > (reinterpret_cast<uint8_t *>(second), deleter);
}

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, int sock) : size(size), bitmask(size - 1) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    mmapRingBuffer(size, "/tmp/rdmaLocal", this->local1, this->local2);
    mmapRingBuffer(size, "/tmp/rdmaRemote", this->remote1, this->remote2);
}
