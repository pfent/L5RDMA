#ifndef RDMA_HASH_MAP_SOCKET_OVERRIDES_H_H
#define RDMA_HASH_MAP_SOCKET_OVERRIDES_H_H

#include <sys/types.h>

/******************** DEFINITIONS ********************/

typedef struct sockaddr sockaddr;
typedef struct msghdr msghdr;

typedef unsigned int socklen_t;

/******************** COMMON OVERRIDES ********************/

int getsockopt(int fd, int level, int option_name, void *option_value, socklen_t *option_len);

int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len);

#endif //RDMA_HASH_MAP_SOCKET_OVERRIDES_H_H
