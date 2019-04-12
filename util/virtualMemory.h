#ifndef L5RDMA_VIRTUALMEMORY_H
#define L5RDMA_VIRTUALMEMORY_H

#include <memory>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <sys/file.h>

namespace l5 {
namespace util {
class WraparoundBuffer {
   public:
   int fd = -1;
   std::shared_ptr<uint8_t> data;

   WraparoundBuffer() = default;

   WraparoundBuffer(int fd, std::shared_ptr<uint8_t> data) : fd(fd), data(std::move(data)) {}

   WraparoundBuffer(const WraparoundBuffer& other) = delete;

   WraparoundBuffer(WraparoundBuffer&& other) noexcept : fd(other.fd), data(std::move(other.data)) {
      other.fd = -1;
   }

   WraparoundBuffer& operator=(const WraparoundBuffer& other) = delete;

   WraparoundBuffer& operator=(WraparoundBuffer&& other) noexcept {
      std::swap(fd, other.fd);
      std::swap(data, other.data);
      return *this;
   }

   ~WraparoundBuffer() { if(fd > 0) ::close(fd); }
};

template <typename T>
class ShmMapping {
public:
    int fd = -1;
    std::shared_ptr<T> data;

    ShmMapping() = default;

    ShmMapping(int fd, std::shared_ptr<T> data) : fd(fd), data(std::move(data)) {}

    ShmMapping(const ShmMapping& other) = delete;

    ShmMapping(ShmMapping&& other) noexcept : fd(other.fd), data(std::move(other.data)) {
        other.fd = -1;
    }

    ShmMapping& operator=(const ShmMapping& other) = delete;

    ShmMapping& operator=(ShmMapping&& other) noexcept {
       std::swap(fd, other.fd);
       std::swap(data, other.data);
       return *this;
    }

    ~ShmMapping() { if(fd > 0) ::close(fd); }
};

/**
 * see: https://dvdhrm.wordpress.com/2014/06/10/memfd_create2/
 * TODO: in the future maybe check of availability of (Linux 3.17+) `memfd_create` and use it instead of shm_open
 * For reliability, the server should not mmap(2) client's objects for read-access as the client might truncate the file
 * simultaneously, causing SIGBUS on the server. A server can protect itself via SIGBUS-handlers, but sealing is a much
 * simpler way. By requiring F_SEAL_SHRINK, the server can be sure, the file will never shrink.
 */
template<typename T>
ShmMapping<T> malloc_shared(const std::string &name, size_t size, void *addr = nullptr) {
    // create a new mapping in /dev/shm
    const auto fd = shm_open(name.c_str(), O_CREAT | O_TRUNC | O_RDWR | O_EXCL, 0666);
    if (fd < 0) {
        perror("shm_open");
        throw std::runtime_error{"shm_open failed"};
    }
    if (shm_unlink(name.c_str()) < 0) {
        perror("shm_unlink");
        throw std::runtime_error{"shm_unlink failed"};
    }
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        throw std::runtime_error{"ftruncate failed"};
    }

    auto deleter = [size](void *p) {
        munmap(p, size);
    };
    auto ptr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    ::memset(ptr, 0, size);

    return ShmMapping<T>( fd, std::shared_ptr<T>(reinterpret_cast<T *>(ptr), deleter) );
}

template<typename T>
ShmMapping<T> malloc_shared(int fd, size_t size, void *addr = nullptr) {
   auto deleter = [size](void *p) {
      munmap(p, size);
   };
   auto ptr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

   return ShmMapping<T>( fd, std::shared_ptr<T>(reinterpret_cast<T *>(ptr), deleter) );
}

WraparoundBuffer mmapRingBuffer(int fd, size_t size, bool init = false);

WraparoundBuffer mmapSharedRingBuffer(const std::string &name, size_t size, bool init = false);
} // namespace util
} // namespace l5

#endif //L5RDMA_VIRTUALMEMORY_H
