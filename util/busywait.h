#ifndef L5RDMA_BUSYWAIT_H
#define L5RDMA_BUSYWAIT_H

#include <unistd.h>
#include <xmmintrin.h>

namespace {
    template<typename Fun, typename Cond>
    constexpr void busyWait(Fun &&fun, Cond &&cond) {
        do {
            fun();
        } while (cond());
    }

    // Yield depending on the number of retires
    constexpr void yield(int tries) {
        if (__builtin_expect(tries < 512, 1)) {
            // NOOP in 99% of the cases
        } else if (__builtin_expect(tries < 4096, 1)) {
            // pause in almost all useful cases
            _mm_pause();
        } else if (__builtin_expect(tries < 32768, 1)) {
            sched_yield();
        } else {
            usleep(1);
        }
    }

    //static std::vector<int> samples = {};
    template<typename Fun, typename Cond>
    void niceWait(Fun &&fun, Cond &&cond) {
        int tries = 0;
        do {
            yield(tries);
            fun();
            ++tries;
        } while (cond());
        //samples.push_back(tries);
    }

    //void printSamples() {
    //    std::sort(samples.begin(), samples.end());
    //    std::cout << "med: " << samples[samples.size() / 2] << '\n';
    //    std::cout << "90%: " << samples[samples.size() * .9] << '\n';
    //    std::cout << "95%: " << samples[samples.size() * .95] << '\n';
    //    std::cout << "99%: " << samples[samples.size() * .99] << '\n';
    //    std::cout << "99.9%: " << samples[samples.size() * .999] << '\n';
    //}
}

template<typename... Args>
constexpr void loop_while(Args &&... args) {
    return niceWait(std::forward<Args>(args)...);
}

#endif //L5RDMA_BUSYWAIT_H
