#include "realFunctions.h"
#include <dlfcn.h>

long ::real::write(int fd, const void *data, size_t size) {
    using real_write_t = ssize_t (*)(int, const void *, size_t);
    return reinterpret_cast<real_write_t>(dlsym(RTLD_NEXT, "write"))(fd, data, size);
}

long ::real::read(int fd, void *data, size_t size) {
    using real_read_t = ssize_t (*)(int, void *, size_t);
    return reinterpret_cast<real_read_t>(dlsym(RTLD_NEXT, "read"))(fd, data, size);
}

long ::real::send(int fd, const void *buffer, size_t length, int flags) {
    using real_send_t = ssize_t (*)(int, const void *, size_t, int);
    return reinterpret_cast<real_send_t>(dlsym(RTLD_NEXT, "send"))(fd, buffer, length, flags);
}

long ::real::recv(int fd, void *buffer, size_t length, int flags) {
    using real_recv_t = ssize_t (*)(int, void *, size_t, int);
    return reinterpret_cast<real_recv_t>(dlsym(RTLD_NEXT, "recv"))(fd, buffer, length, flags);
}

long ::real::sendmsg(int fd, const struct msghdr *message, int flags) {
    using real_sendmsg_t = ssize_t (*)(int, const struct msghdr *, int);
    return reinterpret_cast<real_sendmsg_t>(dlsym(RTLD_NEXT, "sendmsg"))(fd, message, flags);
}

long ::real::recvmsg(int fd, struct msghdr *message, int flags) {
    using real_recvmsg_t = ssize_t (*)(int, struct msghdr *, int);
    return reinterpret_cast<real_recvmsg_t>(dlsym(RTLD_NEXT, "recvmsg"))(fd, message, flags);
}

long ::real::sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr,
                    socklen_t dest_len) {
    using real_sendto_t = ssize_t (*)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
    return reinterpret_cast<real_sendto_t>(dlsym(RTLD_NEXT, "sendto"))(fd, buffer, length, flags, dest_addr,
                                                                       dest_len);
}

long
::real::recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *address, socklen_t *address_len) {
    using real_recvfrom_t = ssize_t (*)(int, void *, size_t, int, struct sockaddr *, socklen_t *);
    return reinterpret_cast<real_recvfrom_t>(dlsym(RTLD_NEXT, "recvfrom"))(fd, buffer, length, flags, address,
                                                                           address_len);
}

int ::real::accept(int fd, sockaddr *address, socklen_t *length) {
    using real_accept_t = int (*)(int, sockaddr *, socklen_t *);
    return reinterpret_cast<real_accept_t>(dlsym(RTLD_NEXT, "accept"))(fd, address, length);
}

int ::real::connect(int fd, const sockaddr *address, socklen_t length) {
    using real_connect_t = int (*)(int, const sockaddr *, socklen_t);
    return reinterpret_cast<real_connect_t>(dlsym(RTLD_NEXT, "connect"))(fd, address, length);
}

int ::real::close(int fd) {
    using real_close_t = int (*)(int);
    return reinterpret_cast<real_close_t>(dlsym(RTLD_NEXT, "close"))(fd);
}

int ::real::getsockopt(int fd, int level, int option_name, void *option_value, socklen_t *option_len) {
    using real_getsockopt_t = int (*)(int, int, int, void *, socklen_t *);
    return ((real_getsockopt_t) dlsym(RTLD_NEXT, "getsockopt"))
            (fd, level, option_name, option_value, option_len);
}

int ::real::setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len) {
    using real_setsockopt_t = int (*)(int, int, int, const void *, socklen_t);
    return reinterpret_cast<real_setsockopt_t>(dlsym(RTLD_NEXT, "setsockopt"))(fd, level, option_name, option_value,
                                                                               option_len);
}

int ::real::getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    using real_getsockname_t = int (*)(int, struct sockaddr *, socklen_t *);
    return reinterpret_cast<real_getsockname_t>(dlsym(RTLD_NEXT, "getsockname"))(fd, addr, addrlen);
}

int ::real::fcntl_set_flags(int fd, int command, int flag) {
    using real_fcntl_t = int (*)(int, int, ...);
    return reinterpret_cast<real_fcntl_t>(dlsym(RTLD_NEXT, "fcntl"))(fd, command, flag);
}

int ::real::fcntl_get_flags(int fd, int command) {
    using real_fcntl_t = int (*)(int, int, ...);
    return reinterpret_cast<real_fcntl_t>(dlsym(RTLD_NEXT, "fcntl"))(fd, command);
}

int ::real::fork() {
    using real_fork_t = pid_t (*)();
    return reinterpret_cast<real_fork_t>(dlsym(RTLD_NEXT, "fork"))();
}
