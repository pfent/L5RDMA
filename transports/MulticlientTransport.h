#ifndef EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H
#define EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H

#include <rdma/RcQueuePair.h>
#include "rdma/Network.hpp"
#include "rdma/MemoryRegion.h"

class MulticlientTransportServer {
    struct Connection {
        int socket;
        rdma::RcQueuePair qp;
        ibv::workrequest::Simple<ibv::workrequest::Write> answerWr;
    };

    static constexpr size_t MAX_MESSAGESIZE = 512;
    static constexpr size_t MAX_CLIENTS = 256;

    const int listenSock;
    rdma::Network net;
    rdma::CompletionQueuePair &sharedCq;

    rdma::RegisteredMemoryRegion<uint8_t[MAX_MESSAGESIZE]> receives;

    rdma::RegisteredMemoryRegion<char> doorBells;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    size_t sendCounter = 0;

    std::vector<Connection> connections;

    void listen(uint16_t port);

public:
    explicit MulticlientTransportServer(std::string_view port);

    ~MulticlientTransportServer();

    void accept();

    // TODO: take callback lambda, that operates on zero-copy range?

    /// polls all possible clients for incoming messages and copys the first one it finds to "whereTo"
    size_t receive(void *whereTo, size_t maxSize);

    void send(size_t receiverId, const uint8_t *data, size_t size);
};

class MultiClientTransportClient {
    static constexpr size_t MAX_MESSAGESIZE = 512;

    const int sock;
    rdma::Network net;
    rdma::CompletionQueuePair &cq;
    rdma::RcQueuePair qp;

    rdma::RegisteredMemoryRegion<uint8_t> sendBuffer;
    rdma::RegisteredMemoryRegion<char> doorBell;
    rdma::RegisteredMemoryRegion<uint8_t> receiveBuffer;

    ibv::workrequest::Simple<ibv::workrequest::Write> dataWr;
    ibv::workrequest::Simple<ibv::workrequest::Write> doorBellWr;

    void rdmaConnect();

public:
    MultiClientTransportClient();

    void connect(std::string_view whereTo);

    void connect(std::string_view ip, uint16_t port);

    void send(const uint8_t *data, size_t size);

    size_t receive(void *whereTo, size_t maxSize);
};

#endif //EXCHANGABLETRANSPORTS_MULTICLIENTTRANSPORT_H
