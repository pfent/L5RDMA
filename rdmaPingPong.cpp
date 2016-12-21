#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <rdma_tests/rdma/CompletionQueuePair.hpp>
#include <rdma_tests/rdma/QueuePair.hpp>
#include <rdma_tests/rdma/MemoryRegion.hpp>
#include <rdma_tests/rdma/WorkRequest.hpp>
#include <infiniband/verbs.h>
#include "rdma/Network.hpp"
#include "tcpWrapper.h"

using namespace std;
using namespace rdma;

void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair);

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer);

void sendRmrInfo(int sock, MemoryRegion &buffer);

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = ::atoi(argv[2]);

    auto sock = tcp_socket();

    static const size_t MESSAGES = 1024 * 128;
    static const size_t BUFFER_SIZE = 64;
    uint8_t buffer[BUFFER_SIZE]{0};
    uint8_t DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    static_assert(BUFFER_SIZE == sizeof(buffer), "DATA needs the right size ");

    // RDMA networking. The queues are needed on both sides
    Network network;
    CompletionQueuePair completionQueue(network);
    QueuePair queuePair(network, completionQueue);

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        tcp_connect(sock, addr);
        exchangeQPNAndConnect(sock, network, queuePair);

        MemoryRegion localSend(DATA, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteSend;
        MemoryRegion localRecBuffer(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        sendRmrInfo(sock, localRecBuffer);
        receiveAndSetupRmr(sock, remoteSend);

        const auto start = chrono::steady_clock::now();
        for (size_t i = 0; i < MESSAGES; ++i) {
            WriteWorkRequestBuilder(localSend, remoteSend, true)
                    .send(queuePair);
            completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE);
            while (buffer[BUFFER_SIZE - 2] == 0) sched_yield(); // sync with response
            for (size_t j = 0; j < BUFFER_SIZE; ++j) {
                if (buffer[j] != DATA[j]) {
                    throw runtime_error{"expected '1~9', received " + string(begin(buffer), end(buffer))};
                }
            }
            fill(begin(buffer), end(buffer), 0);
        }
        const auto end = chrono::steady_clock::now();
        const auto msTaken = chrono::duration<double, milli>(end - start).count();
        const auto sTaken = msTaken / 1000;
        cout << MESSAGES << " messages exchanged in " << msTaken << "ms" << endl;
        cout << MESSAGES / sTaken << " msg/s" << endl;
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        tcp_bind(sock, addr);
        listen(sock, SOMAXCONN);
        sockaddr_in inAddr;

        auto acced = tcp_accept(sock, inAddr);
        exchangeQPNAndConnect(acced, network, queuePair);

        MemoryRegion localBuffer(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        uint8_t toSend[BUFFER_SIZE];
        MemoryRegion toSendMR(toSend, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteBuffer;
        receiveAndSetupRmr(acced, remoteBuffer);
        sendRmrInfo(acced, localBuffer);

        for (size_t i = 0; i < MESSAGES; ++i) {
            while (buffer[BUFFER_SIZE - 2] == 0) sched_yield();
            copy(begin(buffer), end(buffer), begin(toSend));
            fill(begin(buffer), end(buffer), 0);
            WriteWorkRequestBuilder(toSendMR, remoteBuffer, true)
                    .send(queuePair);
            completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE);
        }

        close(acced);
    }

    close(sock);
    return 0;
}

struct RmrInfo {
    uint32_t bufferKey;
    uintptr_t bufferAddress;
};

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer) {
    RmrInfo rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    rmrInfo.bufferKey = ntohl(rmrInfo.bufferKey);
    rmrInfo.bufferAddress = be64toh(rmrInfo.bufferAddress);
    buffer.key = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
}

void sendRmrInfo(int sock, MemoryRegion &buffer) {
    RmrInfo rmrInfo;
    rmrInfo.bufferKey = htonl(buffer.key->rkey);
    rmrInfo.bufferAddress = htobe64((uint64_t) buffer.address);
    tcp_write(sock, &rmrInfo, sizeof(rmrInfo));
}

void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair) {
    uint32_t qpn = queuePair.getQPN();
    uint32_t qPNbuffer = htonl(qpn);
    tcp_write(sock, &qPNbuffer, sizeof(qPNbuffer)); // Send own qpn to server
    tcp_read(sock, &qPNbuffer, sizeof(qPNbuffer)); // receive qpn
    qpn = ntohl(qPNbuffer);
    const Address address{network.getLID(), qpn};
    queuePair.connect(address);
    cout << "connected to qpn " << qpn << endl;
}

