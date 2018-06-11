#ifndef L5RDMA_PINTHREAD_H
#define L5RDMA_PINTHREAD_H

#include <pthread.h>
#include <stdexcept>
#include <string>

void pinThread(size_t cpu);

void pinThread(size_t cpu) {
    using namespace std::string_literals;

    const auto thread = pthread_self();
    cpu_set_t cpuset{};
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    auto res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        throw std::runtime_error("pthread_setaffinity_np failed with "s + strerror(res));
    }
    res = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        throw std::runtime_error("pthread_getaffinity_np failed with "s + strerror(res));
    }
}

#endif //L5RDMA_PINTHREAD_H
