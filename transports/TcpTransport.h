#ifndef EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>

class TcpTransportServer {
    const int initialSocket;
    int communicationSocket = -1;

public:
    explicit TcpTransportServer(std::string_view port);

    ~TcpTransportServer();

    void listen(uint16_t port);

    void accept();

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

class TcpTransportClient {
    const int socket;

public:
    TcpTransportClient();

    ~ TcpTransportClient();

    void connect(std::string_view file);

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
