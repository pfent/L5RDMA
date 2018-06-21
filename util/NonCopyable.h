#pragma once
namespace l5 {
namespace util {
struct NonCopyable {
    NonCopyable() = default;

    NonCopyable(const NonCopyable &) = delete;

    NonCopyable &operator=(const NonCopyable &) = delete;

    NonCopyable(NonCopyable &&) = default;

    NonCopyable &operator=(NonCopyable &&) = default;
};
} // namespace util
} // namespace l5
