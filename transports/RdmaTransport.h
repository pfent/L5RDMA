#ifndef EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <memory>
#include <exchangeableTransports/datastructures/RDMAMessageBuffer.h>
#include "Transport.h"

class RdmaTransportServer : TransportServer<RdmaTransportServer> {
    const int sock;
    std::unique_ptr<RDMAMessageBuffer> rdma = nullptr;

public:
    explicit RdmaTransportServer(std::string_view port);

    ~RdmaTransportServer();

    void listen(uint16_t port);

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class RdmaTransportClient : TransportClient<RdmaTransportClient> {
    const int sock;
    const uint16_t port;
    std::unique_ptr<RDMAMessageBuffer> rdma = nullptr;

public:
    RdmaTransportClient();

    ~RdmaTransportClient();

    void connect_impl(std::string_view port);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
