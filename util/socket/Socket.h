#pragma once

#include "util/NonCopyable.h"
#include <algorithm>
#include <vector>
#include <sys/socket.h>
#include <poll.h>
#include <cstring>
#include <stdexcept>
#include <string>

namespace l5 {
namespace util {
class Socket : util::NonCopyable {

    int s = -1;

    explicit Socket(int raw);

public:
    Socket() = default;

    static Socket create(int domain = AF_INET, int type = SOCK_STREAM, int protocol = 0);

    static Socket fromRaw(int socket) noexcept;

    ~Socket();

    Socket(Socket &&other) noexcept;

    Socket &operator=(Socket &&other) noexcept;

    int get() const noexcept;

    template<typename SocketIterator>
    static SocketIterator poll_first(SocketIterator begin, SocketIterator end);

    void close() noexcept;
};

template<typename SocketIterator>
SocketIterator Socket::poll_first(SocketIterator begin, SocketIterator end) {
    using namespace std::string_literals;
    assert(std::distance(begin, end) > 0);
    const auto size = static_cast<size_t>(std::distance(begin, end));
    std::vector<pollfd> pollFds;

    std::transform(begin, end, std::back_inserter(pollFds), [](const auto &sock) {
        pollfd res{};
        res.fd = sock.get();
        res.events = POLLIN;
        return res;
    });

    const auto ret = ::poll(pollFds.data(), size, 5 * 1000); // 5 seconds timeout
    if (ret < 0) {
        throw std::runtime_error("Could not poll sockets: "s + ::strerror(errno));
    }
    const auto &readable = std::find_if(pollFds.begin(), pollFds.end(), [](const pollfd &pollFd) {
        return (pollFd.revents & POLLIN) != 0;
    });

    return begin + std::distance(pollFds.begin(), readable);
}
} // namespace util
} // namespace l5
