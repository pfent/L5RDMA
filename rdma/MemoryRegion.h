#ifndef L5RDMA_MEMORYREGION_H
#define L5RDMA_MEMORYREGION_H

#include <memory>
#include <libibverbscpp.h>

namespace rdma {
    template<typename T>
    struct RegisteredMemoryRegion {
        std::vector<T> underlying;
        std::unique_ptr<ibv::memoryregion::MemoryRegion> mr;

        RegisteredMemoryRegion(size_t size, rdma::Network &net, std::initializer_list<ibv::AccessFlag> flags) :
                underlying(size),
                mr(net.registerMr(underlying.data(), underlying.size() * sizeof(T), flags)) {}

        std::vector<T> &get() {
            return underlying;
        }

        T *data() {
            return underlying.data();
        }

        typename std::vector<T>::iterator begin() {
            return std::begin(underlying);
        }

        typename std::vector<T>::iterator end() {
            return std::end(underlying);
        }

        ibv::memoryregion::MemoryRegion &rdmaMr() {
            return *mr;
        }

        ibv::memoryregion::RemoteAddress getAddr() {
            return mr->getRemoteAddress();
        }

        ibv::memoryregion::Slice getSlice() {
            return mr->getSlice();
        }

        ibv::memoryregion::Slice getSlice(uint32_t offset, uint32_t sliceLength) {
            return mr->getSlice(offset, sliceLength);
        }

        RegisteredMemoryRegion(const RegisteredMemoryRegion &) = delete;

        RegisteredMemoryRegion &operator=(const RegisteredMemoryRegion &) = delete;

        RegisteredMemoryRegion(RegisteredMemoryRegion &&) = delete;

        RegisteredMemoryRegion &operator=(RegisteredMemoryRegion &&) = delete;

        ~RegisteredMemoryRegion() = default;
    };
}

#endif //L5RDMA_MEMORYREGION_H
