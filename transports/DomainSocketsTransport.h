#ifndef EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>

class DomainSocketsTransportServer {
    const int initialSocket;
    int communicationSocket = -1;

public:
    explicit DomainSocketsTransportServer(std::string_view file);

    ~DomainSocketsTransportServer();

    void accept();

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

class DomainSocketsTransportClient {
    const int socket;

public:
    explicit DomainSocketsTransportClient();

    ~DomainSocketsTransportClient();

    void connect(std::string_view file);

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_DOMAINSOCKETSTRANSPORT_H
