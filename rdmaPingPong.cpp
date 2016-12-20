#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <rdma_tests/rdma/CompletionQueuePair.hpp>
#include <rdma_tests/rdma/QueuePair.hpp>
#include <rdma_tests/rdma/MemoryRegion.hpp>
#include <rdma_tests/rdma/WorkRequest.hpp>
#include <infiniband/verbs.h>
#include "rdma/Network.hpp"

using namespace std;
using namespace rdma;

int tcp_socket() {
    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw runtime_error{"Could not open socket"};
    }
    return sock;
}

void tcp_connect(int sock, sockaddr_in &addr) {
    if (connect(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw runtime_error{"error connect'ing"};
    }
}

void tcp_write(int sock, void *buffer, size_t size) {
    if (write(sock, buffer, size) < 0) {
        throw runtime_error{"error write'ing"};
    }
}

void tcp_read(int sock, void *buffer, size_t size) {
    if (read(sock, buffer, size) < 0) {
        throw runtime_error{"error read'ing"};
    }
}

void tcp_bind(int sock, sockaddr_in &addr) {
    if (bind(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw runtime_error{"error bind'ing"};
    }
}

void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair);

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer, RemoteMemoryRegion &messageCount);

void sendRmrInfo(int sock, MemoryRegion &buffer, MemoryRegion &messageCount);

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, (sockaddr *) &inAddr, &inAddrLen);
    if (acced < 0) {
        throw runtime_error{"error accept'ing"};
    }
    return acced;
}

static const size_t MESSAGES = 1024 * 1024;

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
    uint8_t buffer[BUFFER_SIZE];
    uint8_t DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    size_t messages = 0;
    size_t fetchResult;
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
        MemoryRegion localMessageCount(&messages, sizeof(messages), network.getProtectionDomain(),
                                       MemoryRegion::Permission::All);
        MemoryRegion localFetchResult(&fetchResult, sizeof(fetchResult), network.getProtectionDomain(),
                                      MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteMessageCount;
        sendRmrInfo(sock, localRecBuffer, localMessageCount);
        receiveAndSetupRmr(sock, remoteSend, remoteMessageCount);

        const auto start = chrono::steady_clock::now();
        for (size_t i = 0; i < MESSAGES; ++i) {
            WriteWorkRequestBuilder(localSend, remoteSend, false)
                    .send(queuePair);
            AtomicFetchAndAddWorkRequestBuilder(localFetchResult, remoteMessageCount, 1, true)
                    .send(queuePair);
            completionQueue.waitForCompletion();
            while (messages == i) sched_yield(); // sync with response
            for (size_t j = 0; j < BUFFER_SIZE; ++j) {
                if (buffer[j] != DATA[j]) {
                    throw runtime_error{"expected '1~9', received " + string(begin(buffer), end(buffer))};
                }
            }
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
        RemoteMemoryRegion remoteBuffer;
        MemoryRegion localMessageCount(&messages, sizeof(messages), network.getProtectionDomain(),
                                       MemoryRegion::Permission::All);
        MemoryRegion localFetchResult(&fetchResult, sizeof(fetchResult), network.getProtectionDomain(),
                                      MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteMessageCount;
        receiveAndSetupRmr(acced, remoteBuffer, remoteMessageCount);
        sendRmrInfo(acced, localBuffer, localMessageCount);

        for (size_t i = 0; i < MESSAGES; ++i) {
            while (messages == i) sched_yield();
            WriteWorkRequestBuilder(localBuffer, remoteBuffer, false)
                    .send(queuePair);
            AtomicFetchAndAddWorkRequestBuilder(localFetchResult, remoteMessageCount, 1, true)
                    .send(queuePair);
            completionQueue.waitForCompletion();
        }

        close(acced);
    }

    close(sock);
    return 0;
}

struct RmrInfo {
    uint32_t bufferKey;
    uintptr_t bufferAddress;
    uint32_t messageCountKey;
    uintptr_t messageCountAddress;
};

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer, RemoteMemoryRegion &messageCount) {
    RmrInfo rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    rmrInfo.bufferKey = ntohl(rmrInfo.bufferKey);
    rmrInfo.messageCountKey = ntohl(rmrInfo.messageCountKey);
    rmrInfo.bufferAddress = be64toh(rmrInfo.bufferAddress);
    rmrInfo.messageCountAddress = be64toh(rmrInfo.messageCountAddress);
    buffer.key = rmrInfo.bufferKey;
    messageCount.key = rmrInfo.messageCountKey;
    buffer.address = rmrInfo.bufferAddress;
    messageCount.address = rmrInfo.messageCountAddress;
}

void sendRmrInfo(int sock, MemoryRegion &buffer, MemoryRegion &messageCount) {
    RmrInfo rmrInfo;
    rmrInfo.bufferKey = htonl(buffer.key->rkey);
    rmrInfo.messageCountKey = htonl(messageCount.key->rkey);
    rmrInfo.bufferAddress = htobe64((uint64_t) buffer.address);
    rmrInfo.messageCountAddress = htobe64((uint64_t) messageCount.address);
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

