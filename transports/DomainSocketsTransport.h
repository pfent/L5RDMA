#ifndef RDMA_SOCKETS_DOMAINSOCKETSTRANSPORT_H
#define RDMA_SOCKETS_DOMAINSOCKETSTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>

class DomainSocketsTransport {
    const int sock;
    const uint16_t port;

public:
    explicit DomainSocketsTransport(std::string_view port);

    ~DomainSocketsTransport();

    void connect(std::string_view ip);

    void listen();

    void accept();

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

#endif //RDMA_SOCKETS_DOMAINSOCKETSTRANSPORT_H
