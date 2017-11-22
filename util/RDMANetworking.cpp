#include <iostream>
#include "RDMANetworking.h"
#include "tcpWrapper.h"
#include <infiniband/verbs.h>

static void exchangeQPNAndConnect(int sock, rdma::Network &network, rdma::QueuePair &queuePair) {
    rdma::Address addr{};
    addr.lid = network.getLID();
    addr.qpn = queuePair.getQPN();
    tcp_write(sock, &addr, sizeof(addr)); // Send own qpn to server
    tcp_read(sock, &addr, sizeof(addr)); // receive qpn
    queuePair.connect(addr);
    std::cout << "connected to qpn " << addr.qpn << " lid: " << addr.lid << std::endl;
}

RDMANetworking::RDMANetworking(int sock) :
        completionQueue(network),
        queuePair(network, completionQueue) {
    tcp_setBlocking(sock); // just set the socket to block for our setup.
    exchangeQPNAndConnect(sock, network, queuePair);
}

void receiveAndSetupRmr(int sock, rdma::RemoteMemoryRegion &buffer, rdma::RemoteMemoryRegion &readPos) {
    RmrInfo rmrInfo{};
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    buffer.key = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
    readPos.key = rmrInfo.readPosKey;
    readPos.address = rmrInfo.readPosAddress;
}

void sendRmrInfo(int sock, const rdma::MemoryRegion &buffer, const rdma::MemoryRegion &readPos) {
    RmrInfo rmrInfo{};
    rmrInfo.bufferKey = buffer.key->rkey;
    rmrInfo.bufferAddress = reinterpret_cast<uintptr_t>(buffer.address);
    rmrInfo.readPosKey = readPos.key->rkey;
    rmrInfo.readPosAddress = reinterpret_cast<uintptr_t>(readPos.address);
    tcp_write(sock, &rmrInfo, sizeof(rmrInfo));
}
