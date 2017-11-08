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

    ~DomainSocketsTransportServer();

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class DomainSocketsTransportClient : public TransportClient<DomainSocketsTransportClient> {
    const int socket;

public:
    DomainSocketsTransportClient();

    ~DomainSocketsTransportClient();

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H
