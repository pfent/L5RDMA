#ifndef EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <memory>
#include <exchangeableTransports/datastructures/RDMAMessageBuffer.h>
#include <exchangeableTransports/datastructures/VirtualRDMARingBuffer.h>
#include "Transport.h"

using RdmaMemoryDatastructure = VirtualRDMARingBuffer; // TODO: make this a template and benchmark the difference

class RdmaTransportServer : public TransportServer<RdmaTransportServer> {
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const int sock;
    std::unique_ptr<RdmaMemoryDatastructure> rdma = nullptr;

public:
    explicit RdmaTransportServer(std::string_view port);

    ~RdmaTransportServer() override;

    void listen(uint16_t port);

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

class RdmaTransportClient : public TransportClient<RdmaTransportClient> {
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const int sock;
    std::unique_ptr<RdmaMemoryDatastructure> rdma = nullptr;

public:
    RdmaTransportClient();

    ~RdmaTransportClient() override;

    void connect_impl(std::string_view port);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

#endif //EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
