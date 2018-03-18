#ifndef EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
#define EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H

#include "../datastructures/VirtualRingBuffer.h"
#include "Transport.h"

using SharedMemoryDatastructure = VirtualRingBuffer;

class SharedMemoryTransportServer : public TransportServer<SharedMemoryTransportServer> {
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const int initialSocket;
    const std::string file;
    const std::string remoteBufferName;
    const std::string remoteReadPosName;
    int communicationSocket = -1;
    std::unique_ptr<SharedMemoryDatastructure> messageBuffer;

public:
    /**
     * Exchange information about the shared memory via the given domain socket
     * @param domainSocket filename of the domain socket
     */
    explicit SharedMemoryTransportServer(std::string_view domainSocket);

    ~SharedMemoryTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class SharedMemoryTransportClient : public TransportClient<SharedMemoryTransportClient> {
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const int socket;
    std::unique_ptr<SharedMemoryDatastructure> messageBuffer;

public:
    SharedMemoryTransportClient();

    ~SharedMemoryTransportClient() override;

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};


#endif //EXCHANGABLETRANSPORTS_SHAREDMEMORYTRANSPORT_H
