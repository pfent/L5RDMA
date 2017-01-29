#include <sys/socket.h>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rdma_tests/RDMAMessageBuffer.h"
#include "realFunctions.h"
#include "overrides.h"

// unordered_map does not like to be 0 initialized, so we can't use it here
static std::map<int, std::unique_ptr<RDMAMessageBuffer>> bridge;
static bool dontCloseRDMA = true; // as long as we can't get rid of the RDMA deallocation errors, don't ever close RDMA connections

static const size_t BUFFER_SIZE = 16 * 1024;

auto getRdmaEnv() {
    static const auto rdmaReachable = getenv("USE_RDMA");
    return rdmaReachable;
}

template<typename T>
void warn(T msg) {
    std::cerr << msg << std::endl;
}

bool isTcpSocket(int socket, bool isServer) {
    int socketType;
    {
        socklen_t option;
        socklen_t option_length = sizeof(option);
        if (real::getsockopt(socket, SOL_SOCKET, SO_TYPE, &option, &option_length) < 0) {
            return false;
        }
        socketType = option;
    }

    int addressLocation;
    {
        struct sockaddr_storage options;
        socklen_t size = sizeof(options);
        if (isServer) {
            if (real::getsockname(socket, (struct sockaddr *) &options, &size) < 0) {
                return false;
            }
        } else {
            if (getpeername(socket, (struct sockaddr *) &options, &size) < 0) {
                return false;
            }
        }
        addressLocation = options.ss_family;
    }

    return socketType == SOCK_STREAM && addressLocation == AF_INET;
}

sockaddr_in getRDMAReachable() {
    sockaddr_in possibleAddr;
    if (getRdmaEnv() == nullptr) {
        std::cerr << "USE_RDMA not set, disabling RDMA socket interception" << std::endl;
        return possibleAddr;
    }

    inet_pton(AF_INET, getRdmaEnv(), &possibleAddr.sin_addr);
    return possibleAddr;
}

bool shouldServerIntercept(int serverSocket, int clientSocket) {
    if (not isTcpSocket(serverSocket, true)) {
        return false;
    }

    auto possibleAddr = getRDMAReachable();

    sockaddr_in connectedAddr;
    {
        socklen_t size = sizeof(connectedAddr);
        getpeername(clientSocket, (struct sockaddr *) &connectedAddr, &size);
    }

    std::cout << "connected to: " << inet_ntoa(connectedAddr.sin_addr) << std::endl;
    std::cout << "RDMA connections possible to: " << inet_ntoa(possibleAddr.sin_addr) << std::endl;
    return connectedAddr.sin_addr.s_addr == possibleAddr.sin_addr.s_addr;
}

bool shouldClientIntercept(int socket) {
    if (not isTcpSocket(socket, false)) {
        return false;
    }

    auto possibleAddr = getRDMAReachable();

    sockaddr_in connectedAddr;
    {
        socklen_t size = sizeof(connectedAddr);
        getpeername(socket, (struct sockaddr *) &connectedAddr, &size);
    }

    std::cout << "connected to: " << inet_ntoa(connectedAddr.sin_addr) << std::endl;
    std::cout << "RDMA connections possible to: " << inet_ntoa(possibleAddr.sin_addr) << std::endl;
    return connectedAddr.sin_addr.s_addr == possibleAddr.sin_addr.s_addr;
}

int accept(int server_socket, sockaddr *address, socklen_t *length) {
    warn("accept");
    int client_socket = real::accept(server_socket, address, length);
    if (client_socket < 0) {
        return ERROR;
    }

    if (not shouldServerIntercept(server_socket, client_socket)) {
        return SUCCESS;
    }

    warn("RDMA accept");
    bridge[client_socket] = std::make_unique<RDMAMessageBuffer>(BUFFER_SIZE, client_socket);
    return client_socket;
}

int connect(int fd, const sockaddr *address, socklen_t length) {
    warn("connect");

    if (real::connect(fd, address, length) == ERROR) {
        if (errno != EINPROGRESS) {
            return ERROR;
        }
        // In case of a non blocking socket, we just poll until it is ready...
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        real::poll(&pfd, 1, -1);
        if ((pfd.revents & POLLERR) != 0) {
            return ERROR;
        }
    }

    if (not shouldClientIntercept(fd)) {
        warn("normal connect");
        return SUCCESS;
    }

    try {
        warn("RDMA connect");
        bridge[fd] = std::make_unique<RDMAMessageBuffer>(BUFFER_SIZE, fd);
    } catch (...) {
        return ERROR;
    }
    return SUCCESS;
}

ssize_t write(int fd, const void *source, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
        // TODO: check if server is still alive
        bridge[fd]->send((uint8_t *) source, requested_bytes);
        return SUCCESS;
    }
    return real::write(fd, source, requested_bytes);
}

ssize_t read(int fd, void *destination, size_t requested_bytes) {
    if (bridge.find(fd) != bridge.end()) {
        // TODO: check if server is still alive
        try {
            return bridge[fd]->receive(destination, requested_bytes);
        } catch (...) {
            warn("something went wrong RDMA reading");
        }
        return ERROR;
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
    if (not dontCloseRDMA) {
        bridge.erase(fd);
    }
//#endif

    return real::close(fd);
}

ssize_t send(int fd, const void *buffer, size_t length, int flags) {
    warn("send");
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
    warn("recv");
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
    warn("sendmsg");
    // This one is hard to implemenet because the `msghdr` struct contains
    // an iovec pointer, which points to an array of iovec structs. Each such
    // struct is then a vector with a starting address and length. The sendmsg
    // call then fills these vectors one by one until the stream is empty or
    // all the vectors have been filled. I don't know how many people use this
    // function, but right now we just support a single buffer and else route
    // the call to the socket itself.
    if (msg->msg_iovlen == 1) {
        return sendto(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags,
                      (struct sockaddr *) msg->msg_name,
                      msg->msg_namelen);
    } else {
        warn("Routing sendmsg to socket (too many buffers)");
        return real::sendmsg(fd, msg, flags);
    }
}

ssize_t recvmsg(int fd, struct msghdr *msg, int flags) {
    warn("recvmsg");
    if (msg->msg_iovlen == 1) {
        return recvfrom(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags,
                        (struct sockaddr *) msg->msg_name,
                        &msg->msg_namelen);
    } else {
        warn("Routing recvmsg to socket (too many buffers)");
        return real::recvmsg(fd, msg, flags);
    }
}

ssize_t
sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    warn("sendto");
    // When the destination address is null, then this should be a stream socket
    if (dest_addr == NULL) {
        return send(fd, buffer, length, flags);
    } else {
        // Connection-less sockets (UDP) sockets never use TSSX anyway
        return real::sendto(fd, buffer, length, flags, dest_addr, addrlen);
    }
}

ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    warn("recvfrom");
    // When the destination address is null, then this should be a stream socket
    if (src_addr == NULL) {
        return recv(fd, buffer, length, flags);
    } else {
        // Connection-Less sockets (UDP) sockets never use TSSX anyway
        return real::recvfrom(fd, buffer, length, flags, src_addr, addrlen);
    }
}

pid_t fork(void) {
    dontCloseRDMA = true;
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
                auto &msgBuf = bridge[fds[i].fd];
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
        } while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >
                 timeout);
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
    warn("fcntl");
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

void _count_tssx_sockets(size_t highest_fd, const DescriptorSets *sets, size_t *tssx_count) {
    *tssx_count = 0;
    for (size_t fd = 0; fd < highest_fd; ++fd) {
        if (_is_in_any_set(fd, sets)) {
            if (bridge.find(fd) != bridge.end()) {
                ++(*tssx_count);
            }
        }
    }
}

int timeval_to_milliseconds(const struct timeval *time) {
    int milliseconds;

    milliseconds = time->tv_sec * 1000;
    milliseconds += time->tv_usec / 1000;

    return milliseconds;
}

static struct pollfd *rs_select_to_poll(int *nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) {
    struct pollfd *fds;
    int fd, i = 0;

    fds = (pollfd *) calloc(*nfds, sizeof(*fds));
    if (!fds)
        return NULL;

    for (fd = 0; fd < *nfds; fd++) {
        if (readfds && FD_ISSET(fd, readfds)) {
            fds[i].fd = fd;
            fds[i].events = POLLIN;
        }

        if (writefds && FD_ISSET(fd, writefds)) {
            fds[i].fd = fd;
            fds[i].events |= POLLOUT;
        }

        if (exceptfds && FD_ISSET(fd, exceptfds))
            fds[i].fd = fd;

        if (fds[i].fd)
            i++;
    }

    *nfds = i;
    return fds;
}

static int rs_poll_to_select(int nfds, struct pollfd *fds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) {
    int i, cnt = 0;

    for (i = 0; i < nfds; i++) {
        if (readfds && (fds[i].revents & (POLLIN | POLLHUP))) {
            FD_SET(fds[i].fd, readfds);
            cnt++;
        }

        if (writefds && (fds[i].revents & POLLOUT)) {
            FD_SET(fds[i].fd, writefds);
            cnt++;
        }

        if (exceptfds && (fds[i].revents & ~(POLLIN | POLLOUT))) {
            FD_SET(fds[i].fd, exceptfds);
            cnt++;
        }
    }
    return cnt;
}

int _forward_to_poll(int nfds, DescriptorSets *sets, struct timeval *timeout) {
    auto pollfds = rs_select_to_poll(&nfds, sets->readfds, sets->writefds, sets->errorfds);
    if (pollfds == NULL) {
        return ERROR;
    }

    auto milliseconds = timeout ? timeval_to_milliseconds(timeout) : -1;

    // The actual forwarding call
    auto number_of_events = poll(pollfds, nfds, milliseconds);

    if (sets->readfds)
        FD_ZERO(sets->readfds);
    if (sets->writefds)
        FD_ZERO(sets->writefds);
    if (sets->errorfds)
        FD_ZERO(sets->errorfds);

    if (number_of_events > 0) {
        number_of_events = rs_poll_to_select(nfds, pollfds, sets->readfds, sets->writefds, sets->errorfds);
    }
    free(pollfds);
    return number_of_events;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    DescriptorSets sets = {readfds, writefds, errorfds};
    size_t tssx_count;
    _count_tssx_sockets(nfds, &sets, &tssx_count);

    if (tssx_count == 0) {
        return real::select(nfds, readfds, writefds, errorfds, timeout);
    }

    return _forward_to_poll(nfds, &sets, timeout);
}
