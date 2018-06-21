#pragma once

#include <datastructures/VirtualRingBuffer.h>
#include "Transport.h"

namespace l5 {
namespace transport {
class SharedMemoryTransportServer : public TransportServer<SharedMemoryTransportServer> {
    const size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const util::Socket initialSocket;
    const std::string file;
    const std::string remoteBufferName;
    const std::string remoteReadPosName;
    util::Socket communicationSocket;
    std::unique_ptr<datastructure::VirtualRingBuffer> messageBuffer;

public:
    /**
     * Exchange information about the shared memory via the given domain socket
     * @param domainSocket filename of the domain socket
     */
    explicit SharedMemoryTransportServer(const std::string &domainSocket);

    ~SharedMemoryTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class SharedMemoryTransportClient : public TransportClient<SharedMemoryTransportClient> {
    const size_t BUFFER_SIZE = 16 * 1024 * 1024;
    const util::Socket socket;
    std::unique_ptr<datastructure::VirtualRingBuffer> messageBuffer;

public:
    SharedMemoryTransportClient();

    ~SharedMemoryTransportClient() override;

    void connect_impl(const std::string &file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};
} // namespace transport
} // namespace l5
