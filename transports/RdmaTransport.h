#ifndef RDMA_SOCKETS_MPITRANSPORT_H
#define RDMA_SOCKETS_MPITRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <memory>
#include <rdma_tests/datastructures/RDMAMessageBuffer.h>

class RdmaTransport {
    const int sock;
    const uint16_t port;
    std::unique_ptr<RDMAMessageBuffer> rdma = nullptr;

public:
    explicit RdmaTransport(std::string_view port);

    ~RdmaTransport();

    void connect(std::string_view ip);

    void listen();

    void accept();

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

#endif //RDMA_SOCKETS_MPITRANSPORT_H
