#ifndef EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <memory>
#include "datastructures/RDMAMessageBuffer.h"
#include "datastructures/VirtualRDMARingBuffer.h"
#include "Transport.h"

using RdmaMemoryDatastructure = VirtualRDMARingBuffer; // TODO: make this a template and benchmark the difference

class RdmaTransportServer : public TransportServer<RdmaTransportServer> {
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const int sock;
    std::unique_ptr<RdmaMemoryDatastructure> rdma = nullptr;

    void listen(uint16_t port);

public:

    explicit RdmaTransportServer(std::string_view port);

    ~RdmaTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    template<typename RangeConsumer>
    void readZC(RangeConsumer &&callback) {
        rdma->receive(std::forward<RangeConsumer>(callback));
    }

    template<typename SizeReturner>
    void writeZC(SizeReturner &&doWork) {
        rdma->send(std::forward<SizeReturner>(doWork));
    }
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

    template<typename RangeConsumer>
    void readZC(RangeConsumer &&callback) {
        rdma->receive(std::forward<RangeConsumer>(callback));
    }

    template<typename SizeReturner>
    void writeZC(SizeReturner &&doWork) {
        rdma->send(std::forward<SizeReturner>(doWork));
    }
};

#endif //EXCHANGABLE_TRANSPORTS_MPITRANSPORT_H
