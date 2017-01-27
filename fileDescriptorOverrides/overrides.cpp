#include <sys/socket.h>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>

#include "rdma_tests/RDMAMessageBuffer.h"
#include "realFunctions.h"
#include "overrides.h"

// unordered_map does not like to be 0 initialized, so we can't use it here
static std::map<int, std::unique_ptr<RDMAMessageBuffer>> bridge;
static bool forked = false;

template<typename T>
void warn(T msg) {
    std::cerr << msg << std::endl;
}

int accept(int server_socket, sockaddr *address, socklen_t *length) {
    int client_socket = real::accept(server_socket, address, length);
    if (client_socket < 0) {
        return ERROR;
    }

    int socketType;
    {
        socklen_t option;
        socklen_t option_length = sizeof(option);
        if (real::getsockopt(server_socket, SOL_SOCKET, SO_TYPE, &option, &option_length) < 0) {
            return ERROR;
        }
        socketType = option;
    }

    int addressLocation;
    {
        struct sockaddr_storage options;
        socklen_t size = sizeof(options);
        if (real::getsockname(server_socket, (struct sockaddr *) &options, &size) < 0) {
            return ERROR;
        }
        addressLocation = options.ss_family;
    }

    if (not(socketType == SOCK_STREAM && addressLocation == AF_INET)) {
        // only handle TCP network sockets with RDMA
        // TODO: probably allow more fine grained control over which socket should be over RDMA
        return SUCCESS;
    }

    try {
        warn("RDMA accept");
        bridge[client_socket] = std::make_unique<RDMAMessageBuffer>(4 * 1024, client_socket);
    } catch (...) {
        return ERROR;
    }
    return client_socket;
}

int connect(int fd, const sockaddr *address, socklen_t length) {
    if (real::connect(fd, address, length) == ERROR) {
        return ERROR;
    }

    // client can directly check sockname
    int socketType;
    {
        socklen_t option;
        socklen_t option_length = sizeof(option);
        if (real::getsockopt(fd, SOL_SOCKET, SO_TYPE, &option, &option_length) < 0) {
            return ERROR;
        }
        socketType = option;
    }

    int addressLocation;
    {
        struct sockaddr_storage options;
        socklen_t size = sizeof(options);
        if (getpeername(fd, (struct sockaddr *) &options, &size) < 0) {
            return ERROR;
        }
        addressLocation = options.ss_family;
    }

    if (not(socketType == SOCK_STREAM && addressLocation == AF_INET)) {
        // only handle TCP network sockets with RDMA
        return SUCCESS;
    }

    try {
        warn("RDMA connect");
        bridge[fd] = std::make_unique<RDMAMessageBuffer>(16 * 1024, fd);
    } catch (...) {
        return ERROR;
    }
    return SUCCESS;
}

ssize_t write(int fd, const void *source, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
        warn("RDMA write");
        // TODO: check if server is still alive
        bridge[fd]->send((uint8_t *) source, requested_bytes);
        return SUCCESS;
    }
    return real::write(fd, source, requested_bytes);
}

ssize_t read(int fd, void *destination, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
        warn("RDMA read");
        // TODO: check if server is still alive
        auto res = bridge[fd]->receive();
        if (res.size() > requested_bytes) {
            return ERROR; // TODO
        }
        memcpy(destination, res.data(), res.size());
        return res.size();
    }
    return real::read(fd, destination, requested_bytes);
}

int close(int fd) {
/* TODO
    // epoll is linux only
#ifdef __linux__
    // These two are definitely mutually exclusive
    if (has_epoll_instance_associated(fd)) {
        close_epoll_instance(fd);
    } else {
        bridge_erase(&bridge, fd);
    }
#else
 */
    if (not forked) {
        bridge.erase(fd);
    }
//#endif

    return real::close(fd);
}

ssize_t send(int fd, const void *buffer, size_t length, int flags) {
// For now: We forward the call to write for a certain set of
// flags, which we chose to ignore. By putting them here explicitly,
// we make sure that we only ignore flags, which are not important.
// For production, we might wanna handle these flags
#ifdef __APPLE__
    if (flags == 0) {
#else
    if (flags == 0 || flags == MSG_NOSIGNAL) {
#endif
        return write(fd, buffer, length);
    } else {
        warn("Routing send to socket (unsupported flags)");
        return real::send(fd, buffer, length, flags);
    }
}

ssize_t recv(int fd, void *buffer, size_t length, int flags) {
#ifdef __APPLE__
    if (flags == 0) {
#else
    if (flags == 0 || flags == MSG_NOSIGNAL) {
#endif
        return read(fd, buffer, length);
    } else {
        warn("Routing recv to socket (unsupported flags)");
        return real::recv(fd, buffer, length, flags);
    }
}

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags) {
    // This one is hard to implemenet because the `msghdr` struct contains
    // an iovec pointer, which points to an array of iovec structs. Each such
    // struct is then a vector with a starting address and length. The sendmsg
    // call then fills these vectors one by one until the stream is empty or
    // all the vectors have been filled. I don't know how many people use this
    // function, but right now we just support a single buffer and else route
    // the call to the socket itself.
    if (msg->msg_iovlen == 1) {
        return sendto(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags, (struct sockaddr *) msg->msg_name,
                      msg->msg_namelen);
    } else {
        warn("Routing sendmsg to socket (too many buffers)");
        return real::sendmsg(fd, msg, flags);
    }
}

ssize_t recvmsg(int fd, struct msghdr *msg, int flags) {
    if (msg->msg_iovlen == 1) {
        return recvfrom(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags, (struct sockaddr *) msg->msg_name,
                        &msg->msg_namelen);
    } else {
        warn("Routing recvmsg to socket (too many buffers)");
        return real::recvmsg(fd, msg, flags);
    }
}

ssize_t
sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    // When the destination address is null, then this should be a stream socket
    if (dest_addr == NULL) {
        return send(fd, buffer, length, flags);
    } else {
        // Connection-less sockets (UDP) sockets never use TSSX anyway
        return real::sendto(fd, buffer, length, flags, dest_addr, addrlen);
    }
}

ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    // When the destination address is null, then this should be a stream socket
    if (src_addr == NULL) {
        return recv(fd, buffer, length, flags);
    } else {
        // Connection-Less sockets (UDP) sockets never use TSSX anyway
        return real::recvfrom(fd, buffer, length, flags, src_addr, addrlen);
    }
}

pid_t fork(void) {
    forked = true;
    return real::fork();
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    auto start = std::chrono::steady_clock::now();
    if (nfds == 0) return 0;

    int event_count = 0;
    std::vector<size_t> tssx_fds, normal_fds;
    for (nfds_t index = 0; index < nfds; ++index) {
        if (bridge.find(fds[index].fd) != bridge.end()) {
            tssx_fds.push_back(index);
        } else {
            normal_fds.push_back(index);
        }
    }

    if (tssx_fds.size() == 0) {
        event_count = real::poll(fds, nfds, timeout);
    } else if (normal_fds.size() == 0) {
        warn("RDMA poll");
        do {
            // Do a full loop over all FDs
            for (auto &i : tssx_fds) {
                auto &msgBuf = bridge[i];
                if (msgBuf->hasData()) {
                    auto inFlag = fds[i].events & POLLIN;
                    if (inFlag != 0) ++event_count;
                    fds[i].revents |= inFlag;
                }
                auto outFlag = fds[i].events & POLLOUT;
                if (outFlag != 0) ++event_count;
                fds[i].revents |= outFlag;
            }
            if (event_count > 0) break;
        } while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() > timeout);
    } else {
        warn("can't do mixed RDMA / TCP yet");
        return ERROR;
    }

    // This is necessary for repeated calls with the same poll structures
    // (the kernel probably does this internally first too)
    for (size_t i = 0; i < nfds; ++i) {
        fds[i].revents = 0;
    }

    return event_count;
}

int fcntl(int fd, int command, ...) {
    va_list args;
    if (bridge.find(fd) != bridge.end()) {
        warn("RDMA fcntl");
        // we can probably support O_NONBLOCK
        return ERROR;
    }
    return real::fcntl(fd, command, args);
}

/***********************************/
// Select stuff here.

typedef struct DescriptorSets {
    fd_set *readfds;
    fd_set *writefds;
    fd_set *errorfds;
} DescriptorSets;

bool _fd_is_set(int fd, const fd_set *set) {
    return set != NULL && FD_ISSET(fd, set);
}

bool _is_in_any_set(int fd, const DescriptorSets *sets) {
    if (_fd_is_set(fd, sets->readfds)) return true;
    if (_fd_is_set(fd, sets->writefds)) return true;
    if (_fd_is_set(fd, sets->errorfds)) return true;

    return false;
}

void _count_tssx_sockets(size_t highest_fd, const DescriptorSets *sets, size_t *lowest_fd, size_t *normal_count,
                         size_t *tssx_count) {
    *normal_count = 0;
    *tssx_count = 0;
    *lowest_fd = highest_fd;

    for (size_t fd = 0; fd < highest_fd; ++fd) {
        if (_is_in_any_set(fd, sets)) {
            if (fd < *lowest_fd) {
                *lowest_fd = fd;
            }
            if (bridge.find(fd) != bridge.end()) {
                ++(*tssx_count);
            } else {
                ++(*normal_count);
            }
        }
    }
}

void _copy_set(fd_set *destination, const fd_set *source) {
    if (source == NULL) {
        FD_ZERO(destination);
    } else {
        *destination = *source;
    }
}

void _clear_set(fd_set *set) {
    if (set != NULL) FD_ZERO(set);
}

void _clear_all_sets(DescriptorSets *sets) {
    _clear_set(sets->readfds);
    _clear_set(sets->writefds);
    _clear_set(sets->errorfds);
}

void _copy_all_sets(DescriptorSets *destination, const DescriptorSets *source) {
    _copy_set(destination->readfds, source->readfds);
    _copy_set(destination->writefds, source->writefds);
    _copy_set(destination->errorfds, source->errorfds);
}

int timeval_to_milliseconds(const struct timeval *time) {
    int milliseconds;

    milliseconds = time->tv_sec * 1000;
    milliseconds += time->tv_usec / 1000;

    return milliseconds;
}

bool _waiting_and_ready_for_select_read(int fd, std::unique_ptr<RDMAMessageBuffer> &session, fd_set *operation) {
    if (!_fd_is_set(fd, operation)) return false;

    return session->hasData();
}

bool _waiting_and_ready_for_select_write(int fd, std::unique_ptr<RDMAMessageBuffer> &, fd_set *operation) {
    if (!_fd_is_set(fd, operation)) return false;

    return true;
}

bool _check_select_events(int fd, std::unique_ptr<RDMAMessageBuffer> &session, DescriptorSets *sets) {
    bool activity = false;

    if (_waiting_and_ready_for_select_read(fd, session, sets->readfds)) {
        activity = true;
    }

    if (_waiting_and_ready_for_select_write(fd, session, sets->writefds)) {
        activity = true;
    }

    return activity;
}

int _select_on_tssx_only(DescriptorSets *sets, size_t, size_t lowest_fd, size_t highest_fd,
                         struct timeval *timeout) {

    auto start = std::chrono::steady_clock::now();
    int ready_count = 0;
    int milliseconds = timeout ? timeval_to_milliseconds(timeout) : -1;

    fd_set readfds, writefds, errorfds;
    DescriptorSets original = {&readfds, &writefds, &errorfds};
    _copy_all_sets(&original, sets);
    _clear_all_sets(sets);

    // Do-while for the case of non-blocking
    // so that we do at least one iteration
    do {
        for (size_t fd = lowest_fd; fd < highest_fd; ++fd) {
            auto &session = bridge[fd];
            if (_check_select_events(fd, session, &original)) ++ready_count;
        }
        if (ready_count > 0) break;
    } while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >
             milliseconds);

    return ready_count;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    DescriptorSets sets = {readfds, writefds, errorfds};
    size_t tssx_count, normal_count, lowest_fd;
    _count_tssx_sockets(nfds, &sets, &lowest_fd, &normal_count, &tssx_count);

    if (normal_count == 0) {
        return _select_on_tssx_only(&sets, tssx_count, lowest_fd, nfds, timeout);
    } else if (tssx_count == 0) {
        return real::select(nfds, readfds, writefds, errorfds, timeout);
    } else {
        warn("can't do mixed RDMA / TCP yet");
        return ERROR;
        //return _forward_to_poll(nfds, &sets, tssx_count + normal_count, timeout);
    }
}
