#include <sys/socket.h>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>

#include "rdma_tests/RDMAMessageBuffer.h"
#include "realFunctions.h"
#include "overrides.h"

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