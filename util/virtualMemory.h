#ifndef EXCHANGABLETRANSPORTS_VIRTUALMEMORY_H
#define EXCHANGABLETRANSPORTS_VIRTUALMEMORY_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <sys/file.h>
#include "domainSocketsWrapper.h"

struct SharedMemoryInfo {
    std::string remoteBufferName;

    /// Exchange shared memory information with the remote side of the socket
    SharedMemoryInfo(int sock, const std::string &bufferName) {
        domain_write(sock, bufferName.c_str(), bufferName.size());
        uint8_t buffer[255];
        size_t readCount = domain_read(sock, buffer, 255);
        this->remoteBufferName = std::string(buffer, buffer + readCount);
    }
};

using WraparoundBuffer = std::shared_ptr<uint8_t>;

template<typename T>
std::shared_ptr<T> malloc_shared(const std::string &name, size_t size, bool init, void *addr = nullptr) {
    // create a new mapping in /dev/shm
    const auto fd = shm_open(name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw std::runtime_error{"shm_open failed"};
    }
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw std::runtime_error{"ftruncate failed"};
    }

    auto deleter = [size](void *p) {
        munmap(p, size);
    };
    auto ptr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd); // no need to keep the fd open after the mapping

    if (init) {
        memset(ptr, 0, size);
    }

    return std::shared_ptr < T > (reinterpret_cast<T *>(ptr), deleter);
}

// see https://github.com/willemt/cbuffer
WraparoundBuffer mmapRingBuffer(int fd, size_t size, bool init);

WraparoundBuffer mmapSharedRingBuffer(const std::string &name, size_t size, bool init = false);

WraparoundBuffer mmapRDMARingBuffer(const std::string &name, size_t size, bool init = false);

#endif //EXCHANGABLETRANSPORTS_VIRTUALMEMORY_H
