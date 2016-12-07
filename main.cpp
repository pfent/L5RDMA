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

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &remoteMemoryRegion, RemoteMemoryRegion &remoteWritePos);

void sendRmrInfo(int sock, MemoryRegion &sharedMemoryRegion, MemoryRegion &sharedWritePos);

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
        MemoryRegion sharedBuffer(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteBuffer;
        uint64_t writePos = 0;
        uint64_t writeAdd = 0;
        uint64_t unused = 0;
        MemoryRegion sharedWritePos(&unused, sizeof(unused), network.getProtectionDomain(),
                                    MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteWritePos;

        receiveAndSetupRmr(sock, remoteBuffer, remoteWritePos);

        // TODO: check if our write request still fits into the buffer, and / or do a wraparound with partial writes
        WriteWorkRequest workRequest;
        workRequest.setLocalAddress(sharedBuffer);
        workRequest.setRemoteAddress(remoteBuffer); // TODO: probably create a new remoteBuffer for each write
        workRequest.setCompletion(true);
        queuePair.postWorkRequest(workRequest);

        completionQueue.waitForCompletion();

        // We probably don't need an explicit wraparound. The uint64_t will overflow, when we write more than
        // 16384 Petabyte == 16 Exabyte. That probably "ought to be enough for anybody" in the near future.
        // Assume we have 100Gb/s transfer, then we'll overflow in (16*1024*1024*1024/(100/8))s == 700 years
        writeAdd = BUFFER_SIZE;
        writePos += writeAdd;
        // AtomicFetchAndAdd to update bytes to read / write count
        AtomicFetchAndAddWorkRequest atomicAddWR;
        atomicAddWR.setLocalAddress(sharedWritePos);
        atomicAddWR.setAddValue(writeAdd);
        atomicAddWR.setRemoteAddress(remoteWritePos);
        atomicAddWR.setCompletion(false); // Don't care about the timing, we keep track of the position locally
        queuePair.postWorkRequest(atomicAddWR);

        cout << "Completed write. Current Pos: " << writePos << endl;
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

        MemoryRegion sharedMR(buffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        uint64_t bufferReadPosition = 0;
        auto sharedBufferWritePosition = make_unique<volatile uint64_t>(0);
        MemoryRegion sharedWritePosMR((void *) sharedBufferWritePosition.get(), sizeof(uint64_t),
                                      network.getProtectionDomain(), MemoryRegion::Permission::All);
        sendRmrInfo(acced, sharedMR, sharedWritePosMR);

        // Spin wait until we have some data
        while (bufferReadPosition == *sharedBufferWritePosition) sched_yield();

        // TODO: do the wraparound
        for (char c : buffer) {
            cout << c;
        }
        bufferReadPosition = *sharedBufferWritePosition;

        close(acced);
    }

    close(sock);
    return 0;
}

struct RmrInfo {
    uint32_t bufferKey;
    uintptr_t bufferAddress;
    static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "Only 64bit platforms supported");
    uint32_t writePosKey;
    uintptr_t writePosAddress;
};

void receiveAndSetupRmr(int sock, RemoteMemoryRegion &remoteMemoryRegion, RemoteMemoryRegion &remoteWritePos) {
    RmrInfo rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    rmrInfo.bufferKey = ntohl(rmrInfo.bufferKey);
    rmrInfo.bufferAddress = be64toh(rmrInfo.bufferAddress);
    rmrInfo.writePosKey = ntohl(rmrInfo.writePosKey);
    rmrInfo.writePosAddress = be64toh(rmrInfo.writePosAddress);
    remoteMemoryRegion.key = rmrInfo.bufferKey;
    remoteMemoryRegion.address = rmrInfo.bufferAddress;
    remoteWritePos.key = rmrInfo.writePosKey;
    remoteWritePos.address = rmrInfo.writePosAddress;
}

void sendRmrInfo(int sock, MemoryRegion &sharedMemoryRegion, MemoryRegion &sharedWritePos) {
    RmrInfo rmrInfo;
    rmrInfo.bufferKey = htonl(sharedMemoryRegion.key->rkey);
    rmrInfo.bufferAddress = htobe64((uint64_t) sharedMemoryRegion.address);
    rmrInfo.writePosKey = htonl(sharedWritePos.key->rkey);
    rmrInfo.writePosAddress = htobe64((uint64_t) sharedWritePos.address);
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

