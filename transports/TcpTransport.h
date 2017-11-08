#ifndef EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include "Transport.h"

class TcpTransportServer : public TransportServer<TcpTransportServer> {
    const int initialSocket;
    int communicationSocket = -1;

public:
    explicit TcpTransportServer(std::string_view port);

    ~TcpTransportServer();

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

private:
    void listen(uint16_t port);
};

class TcpTransportClient : public TransportClient<TcpTransportClient> {
    const int socket;

public:
    TcpTransportClient();

    ~ TcpTransportClient();

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
