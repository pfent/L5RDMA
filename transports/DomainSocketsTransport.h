#ifndef EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>
#include "Transport.h"

class DomainSocketsTransportServer : public TransportServer<DomainSocketsTransportServer> {
    const int initialSocket;
    const std::string file;
    int communicationSocket = -1;

public:
    explicit DomainSocketsTransportServer(std::string_view file);

    ~DomainSocketsTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

class DomainSocketsTransportClient : public TransportClient<DomainSocketsTransportClient> {
    const int socket;

public:
    DomainSocketsTransportClient();

    ~DomainSocketsTransportClient() override;

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

#endif //EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H
