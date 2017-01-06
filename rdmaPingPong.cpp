#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
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

struct RDMANetworking {
    Network network;
    CompletionQueuePair completionQueue;
    QueuePair queuePair;

    RDMANetworking(int sock) :
            completionQueue(network),
            queuePair(network, completionQueue) {
        exchangeQPNAndConnect(sock, network, queuePair);
    }
};

template<size_t BUFFER_SIZE>
class RDMAMessageBuffer {
    static const size_t validity = 0xDEADDEADBEEFBEEF;
public:
    void send(uint8_t *data, size_t length) {
        const size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
        if (sizeToWrite > size) throw std::runtime_error{"data > buffersize!"};
        // TODO: current position and wraparound
        auto *begin = sendBuffer;
        memcpy(begin, &length, sizeof(length));
        memcpy(begin + sizeof(length), data, length);
        memcpy(begin + sizeof(length) + length, &validity, sizeof(validity));

        WriteWorkRequestBuilder(localSend, remoteReceive, true)
                .send(net.queuePair);
        net.completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE); // This probably leaves one completion in the CQ
    }

    vector<uint8_t> receive() {
        volatile uint8_t *begin = receiveBuffer; // TODO: current position and wraparound
        volatile auto *receiveSize = (volatile size_t *) begin;
        while (*receiveSize == 0);
        auto messageSize = *receiveSize;
        // TODO: check if size is in bounds or do wraparound
        volatile auto *receiveValidity = (volatile decltype(validity) *) (begin + sizeof(size_t) + messageSize);
        while (*receiveValidity == 0);
        if (*receiveValidity != validity) throw std::runtime_error{"unexpected validity " + *receiveValidity};
        auto result = vector<uint8_t>(begin + sizeof(size_t), begin + sizeof(size_t) + messageSize);
        fill(begin, begin + sizeof(messageSize) + messageSize + sizeof(validity), 0);
        return move(result);
    }

    RDMAMessageBuffer(int sock) :
            net(sock),
            localSend(sendBuffer, size, net.network.getProtectionDomain(), MemoryRegion::Permission::All),
            localReceive((void *) receiveBuffer, size, net.network.getProtectionDomain(),
                         MemoryRegion::Permission::All) {
        sendRmrInfo(sock, localReceive);
        receiveAndSetupRmr(sock, remoteReceive);
    }

private:
    static const size_t size = BUFFER_SIZE;
    RDMANetworking net;
    volatile uint8_t receiveBuffer[size]{0};
    uint8_t sendBuffer[size];
    MemoryRegion localSend;
    MemoryRegion localReceive;
    RemoteMemoryRegion remoteReceive;
};

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

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        tcp_connect(sock, addr);

        auto sendData = array<uint8_t, 64>{"123456789012345678901234567890123456789012345678901234567890123"};
        RDMAMessageBuffer<128> rdma(sock);

        const auto start = chrono::steady_clock::now();
        for (size_t i = 0; i < MESSAGES; ++i) {
            rdma.send(sendData.data(), sendData.size());
            auto answer = rdma.receive();
            if (answer.size() != sendData.size()) {
                throw runtime_error{"answer has wrong size!"};
            }
            for (size_t j = 0; j < BUFFER_SIZE; ++j) {
                if (answer[j] != sendData[j]) {
                    throw runtime_error{"expected '1~9', received " + string(answer.begin(), answer.end())};
                }
            }
        }
        const auto end = chrono::steady_clock::now();
        const auto msTaken = chrono::duration<double, milli>(end - start).count();
        const auto sTaken = msTaken / 1000;
        cout << MESSAGES << " " << sendData.size() << "B messages exchanged in " << msTaken << "ms" << endl;
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

        RDMAMessageBuffer<128> rdma(acced);

        for (size_t i = 0; i < MESSAGES; ++i) {
            auto ping = rdma.receive();
            rdma.send(ping.data(), ping.size());
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
    rdma::Address addr;
    addr.lid = network.getLID();
    addr.qpn = queuePair.getQPN();
    tcp_write(sock, &addr, sizeof(addr)); // Send own qpn to server
    tcp_read(sock, &addr, sizeof(addr)); // receive qpn
    queuePair.connect(addr);
    cout << "connected to qpn " << addr.qpn << " lid: " << addr.lid << endl;
}

