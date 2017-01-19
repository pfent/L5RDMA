#include <stdio.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <iostream>
#include <unordered_map>
#include <cstring>

#include "socket-overrides.h"
#include "rdma_tests/RDMAMessageBuffer.h"
#include "realFunctions.h"

/**** GLOBAL *****/

#define ERROR -1
#define SUCCESS 0

std::unordered_map<int, std::unique_ptr<RDMAMessageBuffer>> bridge;

template<typename T>
void warn(T msg) {
    std::cerr << msg << std::endl;
}

/******************** CLIENT OVERRIDES ********************/

// socket() will be directly called by the user, so we already start with a ready to use socket

extern "C"
int connect(int fd, const sockaddr *address, socklen_t length) {
    if (real::connect(fd, address, length) == ERROR) {
        return ERROR;
    }

    // client can directly check sockname
    int socketType;
    {
        socklen_t option;
        socklen_t option_length = sizeof(option);
        socketType = real::getsockopt(fd, SOL_SOCKET, SO_TYPE, &option, &option_length);
        if (socketType < 0) {
            return ERROR;
        }
    }

    int addressLocation;
    {
        struct sockaddr_storage options;
        socklen_t size = sizeof(options);
        if (real::getsockname(fd, (struct sockaddr *) &options, &size) < 0) {
            return ERROR;
        }
        addressLocation = options.ss_family;
    }

    if (not(socketType == SOCK_STREAM && addressLocation == AF_INET)) {
        // only handle TCP network sockets with RDMA
        return SUCCESS;
    }

    try {
        bridge[fd] = std::make_unique<RDMAMessageBuffer>(4 * 1024, fd);
    } catch (...) {
        return ERROR;
    }
    return SUCCESS;
}

extern "C"
ssize_t write(int fd, const void *source, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
        // TODO: check if server is still alive
        bridge[fd]->send((uint8_t *) source, requested_bytes);
        return SUCCESS;
    }
    return real::write(fd, source, requested_bytes);
}

extern "C"
ssize_t read(int fd, void *destination, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
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

/******************** COMMON OVERRIDES ********************/

extern "C"
int getsockopt(int fd, int level, int option_name, void *option_value, socklen_t *option_len) {
    return real::getsockopt(fd, level, option_name, option_value, option_len);
}

extern "C"
int getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    return real::getsockname(fd, addr, addrlen);
}

extern "C"
int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len) {
    return real::setsockopt(fd, level, option_name, option_value, option_len);
}

extern "C"
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
    bridge.erase(fd);
//#endif

    return real::close(fd);
}

extern "C"
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

extern "C"
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

extern "C"
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

extern "C"
ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    // When the destination address is null, then this should be a stream socket
    if (src_addr == NULL) {
        return recv(fd, buffer, length, flags);
    } else {
        // Connection-Less sockets (UDP) sockets never use TSSX anyway
        return real::recvfrom(fd, buffer, length, flags, src_addr, addrlen);
    }
}

extern "C"
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

extern "C"
ssize_t recvmsg(int fd, struct msghdr *msg, int flags) {
    if (msg->msg_iovlen == 1) {
        return recvfrom(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags, (struct sockaddr *) msg->msg_name,
                        &msg->msg_namelen);
    } else {
        warn("Routing recvmsg to socket (too many buffers)");
        return real::recvmsg(fd, msg, flags);
    }
}
