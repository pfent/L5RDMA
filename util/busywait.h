#ifndef L5RDMA_BUSYWAIT_H
#define L5RDMA_BUSYWAIT_H

#include <unistd.h>
#include <xmmintrin.h>

namespace {
    template<typename Fun, typename Cond>
    void busyWait(Fun &&fun, Cond &&cond) {
        do {
            fun();
        } while (cond());
    }

    // TODO: those values are a bit arbitrary, maybe more intelligence here?
    // E.g. sample expected and deviation and set pause / yield accordingly
    void yield(int tries) {
        if (tries < 2) { // nop
        } else if (tries < 64) {
            _mm_pause();
        } else if (tries < 128) {
            sched_yield();
        } else {
            usleep(1);
        }
    }

    template<typename Fun, typename Cond>
    void niceWait(Fun &&fun, Cond &&cond) {
        int tries = 0;
        do {
            yield(tries);
            fun();
            ++tries;
        } while (cond());
    }
}

template<typename... Args>
void loop_while(Args &&... args) {
    return niceWait(std::forward<Args>(args)...);
}

#endif //L5RDMA_BUSYWAIT_H
