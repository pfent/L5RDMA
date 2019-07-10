#pragma once

#include "util/socket/Socket.h"
#include "Transport.h"

namespace l5 {
namespace transport {
class TcpTransportServer : public TransportServer<TcpTransportServer> {
    const util::Socket initialSocket;
    util::Socket communicationSocket;

public:
    explicit TcpTransportServer(const std::string &port);

    ~TcpTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    size_t readSome_impl(uint8_t *buffer, size_t maxSize);

private:
    void listen(uint16_t port);
};

class TcpTransportClient : public TransportClient<TcpTransportClient> {
    const util::Socket socket;

public:
    TcpTransportClient();

    ~ TcpTransportClient() override;

    void connect_impl(const std::string &file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    size_t readSome_impl(uint8_t *buffer, size_t maxSize);
};
} // namespace transport
} // namespace l5
