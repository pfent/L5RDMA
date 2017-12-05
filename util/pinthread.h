#ifndef EXCHANGABLETRANSPORTS_PINTHREAD_H
#define EXCHANGABLETRANSPORTS_PINTHREAD_H

#include <pthread.h>
#include <stdexcept>

void pinThread(int cpu) {
    auto thread = pthread_self();
    cpu_set_t cpuset{};
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
        throw std::runtime_error("pthread_setaffinity_np failed");
    }
    if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_getaffinity_np");
        throw std::runtime_error("pthread_getaffinity_np failed");
    }
}

#endif //EXCHANGABLETRANSPORTS_PINTHREAD_H
