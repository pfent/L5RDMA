#include <cstring>
#include <unistd.h>
#include "Socket.h"

using namespace std::string_literals;

l5::util::Socket::~Socket() {
    if (s < 0) {
        return;
    }
    ::close(s);
}

l5::util::Socket::Socket(l5::util::Socket &&other) noexcept {
    std::swap(s, other.s);
}

l5::util::Socket &l5::util::Socket::operator=(l5::util::Socket &&other) noexcept {
    std::swap(s, other.s);
    return *this;
}

int l5::util::Socket::get() const noexcept {
    return s;
}

l5::util::Socket l5::util::Socket::fromRaw(int socket) noexcept {
    return Socket(socket);
}

l5::util::Socket l5::util::Socket::create(int domain, int type, int protocol) {
    auto raw = ::socket(domain, type, protocol);
    if (raw < 0) {
        throw std::runtime_error("Could not open socket: "s + strerror(errno));
    }
    const int enable = 1;
    if (::setsockopt(raw, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        throw std::runtime_error{"Could not set SO_REUSEADDR: "s + strerror(errno)};
    }
    return Socket(raw);
}

l5::util::Socket::Socket(int raw) : s(raw) {
}

void l5::util::Socket::close() noexcept {
    if (s < 0) {
        return;
    }
    ::close(s);
    s = -1;
}
