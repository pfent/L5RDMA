#include "RDMAMessageBuffer.h"
#include <cstring>
#include <iostream>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include "rdma/WorkRequest.hpp"
#include "tcpWrapper.h"

using namespace std;
using namespace rdma;

static inline void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
    } rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    rmrInfo.bufferKey = ntohl(rmrInfo.bufferKey);
    rmrInfo.bufferAddress = be64toh(rmrInfo.bufferAddress);
    buffer.key = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
}

static inline void sendRmrInfo(int sock, MemoryRegion &buffer) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
    } rmrInfo;
    rmrInfo.bufferKey = htonl(buffer.key->rkey);
    rmrInfo.bufferAddress = htobe64((uint64_t) buffer.address);
    tcp_write(sock, &rmrInfo, sizeof(rmrInfo));
}

static inline void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair) {
    Address addr;
    addr.lid = network.getLID();
    addr.qpn = queuePair.getQPN();
    tcp_write(sock, &addr, sizeof(addr)); // Send own qpn to server
    tcp_read(sock, &addr, sizeof(addr)); // receive qpn
    queuePair.connect(addr);
    cout << "connected to qpn " << addr.qpn << " lid: " << addr.lid << endl;
}

vector<uint8_t> RDMAMessageBuffer::receive() {
    volatile uint8_t *begin = receiveBuffer.get(); // TODO: current position and wraparound
    volatile auto *receiveSize = (volatile size_t *) begin;
    while (*receiveSize == 0);
    auto messageSize = *receiveSize;
    // TODO: check if size is in bounds or do wraparound
    volatile auto *receiveValidity = (volatile decltype(validity) *) (begin + sizeof(size_t) + messageSize);
    while (*receiveValidity == 0);
    if (*receiveValidity != validity) throw runtime_error{"unexpected validity " + *receiveValidity};
    auto result = vector<uint8_t>(begin + sizeof(size_t), begin + sizeof(size_t) + messageSize);
    fill(begin, begin + sizeof(messageSize) + messageSize + sizeof(validity), 0);
    return move(result);
}

RDMAMessageBuffer::RDMAMessageBuffer(size_t size, int sock) :
        size(size),
        net(sock),
        receiveBuffer(new volatile uint8_t[size]()),
        sendBuffer(new uint8_t[size]()),
        localSend(sendBuffer.get(), size, net.network.getProtectionDomain(), MemoryRegion::Permission::All),
        localReceive((void *) receiveBuffer.get(), size, net.network.getProtectionDomain(),
                     MemoryRegion::Permission::All) {
    sendRmrInfo(sock, localReceive);
    receiveAndSetupRmr(sock, remoteReceive);
}

void RDMAMessageBuffer::send(uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};
    // TODO: current position and wraparound
    auto *begin = sendBuffer.get();
    memcpy(begin, &length, sizeof(length));
    memcpy(begin + sizeof(length), data, length);
    memcpy(begin + sizeof(length) + length, &validity, sizeof(validity));

    WriteWorkRequestBuilder(localSend, remoteReceive, true)
            .send(net.queuePair);
    net.completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE); // This probably leaves one completion in the CQ
}

RDMANetworking::RDMANetworking(int sock) :
        completionQueue(network),
        queuePair(network, completionQueue) {
    exchangeQPNAndConnect(sock, network, queuePair);
}
