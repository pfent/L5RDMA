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

    ~RdmaTransportServer() override;

    void listen(uint16_t port);

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer &buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer &readBuffer);
};

class RdmaTransportClient : TransportClient<RdmaTransportClient> {
    const int sock;
    const uint16_t port;
    std::unique_ptr<RDMAMessageBuffer> rdma = nullptr;

public:
    RdmaTransportClient();

    ~RdmaTransportClient() override;

    void connect_impl(std::string_view port);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer &buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer &readBuffer);
};

#endif //EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
