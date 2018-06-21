#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <util/socket/Socket.h>
#include "Transport.h"

namespace l5 {
namespace transport {
class DomainSocketsTransportServer : public TransportServer<DomainSocketsTransportServer> {
    const util::Socket initialSocket;
    const std::string file;
    util::Socket communicationSocket;

public:
    explicit DomainSocketsTransportServer(std::string file);

    ~DomainSocketsTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class DomainSocketsTransportClient : public TransportClient<DomainSocketsTransportClient> {
    const util::Socket socket;

public:
    DomainSocketsTransportClient();

    ~DomainSocketsTransportClient() override;

    void connect_impl(std::string file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};
} // namespace transport
} // namespace l5
