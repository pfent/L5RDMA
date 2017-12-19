#ifndef EXCHANGABLETRANSPORTS_LIBRDMACMTRANSPORT_H
#define EXCHANGABLETRANSPORTS_LIBRDMACMTRANSPORT_H

#include "Transport.h"
#include <rdma/rsocket.h>

class LibRdmacmTransportServer : public TransportServer<LibRdmacmTransportServer> {
    int rdmaSocket;
    int commSocket = -1;

public:
    explicit LibRdmacmTransportServer(std::string_view port);

    ~LibRdmacmTransportServer() override;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

class LibRdmaTransportClient : public TransportClient<LibRdmaTransportClient> {
    int rdmaSocket = -1;

public:
    LibRdmaTransportClient();

    ~LibRdmaTransportClient() override;

    void connect_impl(std::string_view file);

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};


#endif //EXCHANGABLETRANSPORTS_LIBRDMACMTRANSPORT_H
