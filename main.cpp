#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
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

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &remoteMemoryRegion);
void sendRmrInfo(int sock, MemoryRegion &sharedMemoryRegion);

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, (sockaddr *) &inAddr, &inAddrLen);
    if (acced < 0) {
        throw runtime_error{"error accept'ing"};
    }
    return acced;
}

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = ::atoi(argv[2]);

    auto sock = tcp_socket();

    const size_t BUFFER_SIZE = 64;
    uint8_t buffer[BUFFER_SIZE];
    const char DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    static_assert(BUFFER_SIZE == sizeof(DATA), "DATA needs the right size ");

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

        copy(begin(DATA), end(DATA), begin(buffer)); // Send DATA
        MemoryRegion sharedMR(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteMemoryRegion;
        receiveAndSetupRmr(sock, remoteMemoryRegion);

        WriteWorkRequest workRequest;
        workRequest.setLocalAddress(sharedMR);
        workRequest.setRemoteAddress(remoteMemoryRegion);
        workRequest.setCompletion(true);

        queuePair.postWorkRequest(workRequest);
        auto a = completionQueue.waitForCompletion();
        cout << "Completed: " << a.first << " " << a.second << endl;
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        tcp_bind(sock, addr);
        listen(sock, SOMAXCONN);
        sockaddr_in inAddr;
        for (;;) {
            auto acced = tcp_accept(sock, inAddr);
            exchangeQPNAndConnect(acced, network, queuePair);

            MemoryRegion sharedMR(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
            sendRmrInfo(acced, sharedMR);

            // TODO: maintain a bytes to read / bytes to write count
            int qwe;
            cout << "waiting for data ..." << endl;
            cin >> qwe;

            cout << buffer;

            close(acced);
        }
    }

    close(sock);
    return 0;
}

struct RmrInfo {
    uint32_t key;
    uintptr_t address;
    static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "Only 64bit platforms supported");
};

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &remoteMemoryRegion) {
    RmrInfo rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    rmrInfo.key = ntohl(rmrInfo.key);
    rmrInfo.address = be64toh(rmrInfo.address);
    remoteMemoryRegion.key = rmrInfo.key;
    remoteMemoryRegion.address = rmrInfo.address;
}

void sendRmrInfo(int sock, MemoryRegion &sharedMemoryRegion) {
    RmrInfo rmrInfo;
    rmrInfo.key = htonl(sharedMemoryRegion.key->rkey);
    rmrInfo.address = htobe64((uint64_t) sharedMemoryRegion.address);
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

