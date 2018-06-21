#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <util/socket/Socket.h>
#include <datastructures/VirtualRDMARingBuffer.h>
#include "Transport.h"

namespace l5 {
namespace transport {
class RdmaTransportServer : public TransportServer<RdmaTransportServer> {
    const size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const util::Socket sock;
    std::unique_ptr<datastructure::VirtualRDMARingBuffer> rdma = nullptr;

    void listen(uint16_t port);

public:

    explicit RdmaTransportServer(const std::string &port);

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
    const size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const util::Socket sock;
    std::unique_ptr<datastructure::VirtualRDMARingBuffer> rdma = nullptr;

public:
    RdmaTransportClient();

    ~RdmaTransportClient() override;

    void connect_impl(const std::string &port);

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
} // namespace transport
} // namespace l5
