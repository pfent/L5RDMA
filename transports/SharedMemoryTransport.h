#ifndef EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
#define EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H


#include "Transport.h"

class SharedMemoryTransportServer : public TransportServer<SharedMemoryTransportServer> {
    const int initialSocket;
    int communicationSocket = -1;

public:
    SharedMemoryTransportServer(std::string_view port);

    ~SharedMemoryTransportServer();

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

private:
    void listen(uint16_t port);
};

class SharedMemoryTransportClient : public TransportClient<SharedMemoryTransportClient> {
    const int socket;

public:
    SharedMemoryTransportClient();

    ~SharedMemoryTransportClient();

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};


#endif //EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
