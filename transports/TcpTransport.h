#ifndef EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
#define EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H

#include <cstdint>
#include <cstddef>
#include <string_view>

class TcpTransport {
    const int initialSocket;
    const uint16_t port;
    int communicationSocket = -1;

public:
    explicit TcpTransport(std::string_view port);

    ~TcpTransport();

    void connect(std::string_view ip);

    void listen();

    void accept();

    void write(const uint8_t *data, size_t size);

    void read(uint8_t *buffer, size_t size);
};

#endif //EXCHANGABLE_TRANSPORTS_TCPTRANSPORT_H
