#include "virtualMemory.h"

namespace l5 {
namespace util {
WraparoundBuffer mmapSharedRingBuffer(const std::string &name, size_t size, bool init) {
    // create a new mapping in /dev/shm
    auto pos = name.rfind('/');
    if (pos == std::string::npos) {
        pos = 0;
    }
    const auto fd = shm_open(name.c_str() + pos, O_CREAT | O_TRUNC | O_RDWR | O_EXCL, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw std::runtime_error{"shm_open failed"};
    }
    if (shm_unlink(name.c_str()) < 0) {
        perror("shm_unlink");
        throw std::runtime_error{"shm_unlink failed"};
    }
    return mmapRingBuffer(fd, size, init);
}

// see https://github.com/willemt/cbuffer
WraparoundBuffer mmapRingBuffer(int fd, size_t size, bool init) {
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw std::runtime_error{"ftruncate failed"};
    }

    // first acquire enough continuous memory
    auto ptr = reinterpret_cast<uint8_t *>(
            mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
    );
    const auto deleter = [size](void *p) {
        // unmap the whole continuous memory mapping
        munmap(p, size * 2);
    };

    // map the shared memory to the first half
    if (mmap(ptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) != ptr) {
        throw std::runtime_error{std::string("mmaping the first wraparound failed ") + strerror(errno)};
    }
    // and also to the second half
    if (mmap(&ptr[size], size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) != &ptr[size]) {
        throw std::runtime_error{std::string("mmaping the second wraparound failed ") +  + strerror(errno)};
    }
    // because of the overlap, we need no deleter and unmapping for those mappings

    if (init) {
        memset(ptr, 0, size);
    }

    return WraparoundBuffer(fd, {ptr, deleter});
}
} // namespace util
} // namespace l5
