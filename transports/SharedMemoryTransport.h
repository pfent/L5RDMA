#ifndef EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
#define EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H


#include "Transport.h"

class SharedMemoryTransportServer : public TransportServer<SharedMemoryTransportServer> {
    const std::string remoteBufferName;
    const std::string remoteReadPosName;

public:
    /**
     * Exchange information about the shared memory via the given domain socket
     * @param domainSocket filename of the domain socket
     */
    SharedMemoryTransportServer(std::string_view domainSocket);

    ~SharedMemoryTransportServer();

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

private:
    void listen(uint16_t port);
};

class SharedMemoryTransportClient : public TransportClient<SharedMemoryTransportClient> {
    const std::string remoteBufferName;
    const std::string remoteReadPosName;

public:
    SharedMemoryTransportClient();

    ~SharedMemoryTransportClient();

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};


#endif //EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
