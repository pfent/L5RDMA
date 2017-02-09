#ifndef OVERRIDES_H
#define OVERRIDES_H
#pragma GCC visibility push(default)

#define ERROR -1
#define SUCCESS 0

/**
 * This is where all the overrides for the file descriptor syscalls are.
 * We override all "important" syscalls, so we can intercept messages that would be sent via TCP to a host also
 * reachable by RDMA. We then don't just proxy the syscall, but for read()/write() we can mirror them with RDMA, so we
 * don't actually do the syscall, thus significantly reducing latency.
 * We don't need the socket() and listen() syscalls, since the default behaviour is perfectly fine.
 */

extern "C" {
int accept(int server_socket, sockaddr *address, socklen_t *length);

int connect(int fd, const sockaddr *address, socklen_t length);

int close(int fd);

ssize_t write(int fd, const void *source, size_t requested_bytes);

ssize_t read(int fd, void *destination, size_t requested_bytes);

ssize_t recv(int fd, void *buffer, size_t length, int flags);

ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

ssize_t recvmsg(int fd, struct msghdr *msg, int flags);

ssize_t send(int fd, const void *buffer, size_t length, int flags);

ssize_t
sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags);

pid_t fork(void);

int getsockopt(int fd, int level, int option_name, void *option_value,
               socklen_t *option_len) __THROW; // Mark them as throw in c++ but not in c context

int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len) __THROW;

int fcntl(int fd, int command, ...);

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);
}

#pragma GCC visibility pop
#endif //OVERRIDES_H
