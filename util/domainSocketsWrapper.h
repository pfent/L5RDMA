#ifndef L5RDMA_DOMAINSOCKETSWRAPPER_H
#define L5RDMA_DOMAINSOCKETSWRAPPER_H

#include <cstddef>
#include <string_view>

struct sockaddr_un;

int domain_socket();

void domain_listen(int sock);

void domain_connect(int sock, std::string_view pathToFile);

void domain_write(int sock, const void *buffer, std::size_t size);

size_t domain_read(int sock, void *buffer, std::size_t size);

void domain_bind(int sock, std::string_view pathToFile);

int domain_accept(int sock, sockaddr_un &inAddr);

void domain_setBlocking(int sock);

void domain_close(int sock);

void domain_unlink(std::string_view pathToFile);

#endif //L5RDMA_DOMAINSOCKETSWRAPPER_H
