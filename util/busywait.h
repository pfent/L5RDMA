#ifndef EXCHANGABLETRANSPORTS_BUSYWAIT_H
#define EXCHANGABLETRANSPORTS_BUSYWAIT_H

#include <xmmintrin.h>

template<typename Fun, typename Cond>
void busyWait(Fun &&fun, Cond &&cond) {
    do { // TODO: don't busy wait
        fun();
    } while (cond());
}

void yield(int tries) {
    if (tries < 4) { // nop
    } else if (tries < 16) {
        _mm_pause();
    } else if (tries < 32) {
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

template<typename... Args>
void loop_while(Args &&... args) {
    return busyWait(std::forward<Args>(args)...);
}

#endif //EXCHANGABLETRANSPORTS_BUSYWAIT_H
