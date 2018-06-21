#ifndef L5RDMA_LIBRDMACMTRANSPORT_H
#define L5RDMA_LIBRDMACMTRANSPORT_H

#include "Transport.h"
#include <linux/kernel.h>
#include <rdma/rsocket.h>

namespace l5 {
namespace transport {
class LibRdmacmTransportServer : public TransportServer<LibRdmacmTransportServer> {
    int rdmaSocket;
    int commSocket = -1;

public:
    explicit LibRdmacmTransportServer(std::string_view port);

    ~LibRdmacmTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};

class LibRdmacmTransportClient : public TransportClient<LibRdmacmTransportClient> {
    int rdmaSocket = -1;

public:
    LibRdmacmTransportClient();

    ~LibRdmacmTransportClient() override;

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);
};
} // namespace transport
} // namespace l5

#endif //L5RDMA_LIBRDMACMTRANSPORT_H
