#ifndef L5RDMA_DONOTOPTIMIZE_H
#define L5RDMA_DONOTOPTIMIZE_H

// see: https://github.com/google/benchmark/blob/master/include/benchmark/benchmark.h

#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__pnacl__) || \
    defined(__EMSCRIPTEN__)
#define BENCHMARK_HAS_NO_INLINE_ASSEMBLY
#endif

// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#ifndef BENCHMARK_HAS_NO_INLINE_ASSEMBLY
template <class Tp>
inline __attribute__((always_inline)) void DoNotOptimize(Tp const& value) {
   asm volatile("" : : "r,m"(value) : "memory");
}

template <class Tp>
inline __attribute__((always_inline)) void DoNotOptimize(Tp& value) {
#if defined(__clang__)
   asm volatile("" : "+r,m"(value) : : "memory");
#else
   asm volatile("" : "+m,r"(value) : : "memory");
#endif
}

// Force the compiler to flush pending writes to global memory. Acts as an
// effective read/write barrier
inline __attribute__((always_inline)) void ClobberMemory() {
   asm volatile("" : : : "memory");
}
#elif defined(_MSC_VER)
template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
  _ReadWriteBarrier();
}

inline BENCHMARK_ALWAYS_INLINE void ClobberMemory() { _ReadWriteBarrier(); }
#else
template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
}
// FIXME Add ClobberMemory() for non-gnu and non-msvc compilers
#endif

#endif //L5RDMA_DONOTOPTIMIZE_H
