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
#include <stdlib.h>
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

    const size_t BUFFER_SIZE = 64 * 4; // TODO: buffersize, that forces wraparound
    const char DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    static_assert(64 == sizeof(DATA), "DATA needs the right size ");

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

        const size_t completeBufferSize = BUFFER_SIZE;
        uint64_t writePos = 0;
        volatile uint64_t readPos = 0;
        uint8_t localBuffer[completeBufferSize]{};
        uint64_t lastWrite = 0;

        MemoryRegion sharedBuffer(localBuffer, completeBufferSize, network.getProtectionDomain(),
                                  MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteBuffer;
        MemoryRegion sharedWritePos(&lastWrite, sizeof(lastWrite), network.getProtectionDomain(),
                                    MemoryRegion::Permission::All);
        MemoryRegion sharedReadPos((void *) &readPos, sizeof(readPos), network.getProtectionDomain(),
                                   MemoryRegion::Permission::All);
        RemoteMemoryRegion remoteWritePos;
        RemoteMemoryRegion remoteReadPos; // TODO: setup remote read pos
        receiveAndSetupRmr(sock, remoteBuffer, remoteWritePos);

        for (int i = 0; i < 4; ++i) {
            // Send DATA
            const size_t sizeToWrite = sizeof(DATA);
            size_t safeToWrite = completeBufferSize - (writePos - readPos);
            while (safeToWrite < sizeToWrite) {
                ReadWorkRequest readWritePos;
                readWritePos.setLocalAddress(sharedReadPos);
                readWritePos.setRemoteAddress(remoteReadPos);
                readWritePos.setCompletion(true);
                queuePair.postWorkRequest(readWritePos);
                completionQueue.waitForCompletionReceive(); // Synchronization point
                safeToWrite = completeBufferSize - (writePos - readPos);
            }
            const size_t beginPos = writePos;
            const size_t endPos = (writePos + sizeToWrite) % completeBufferSize;
            if (endPos <= beginPos) {
                // TODO: split and wraparound
                // TODO: check if our write request still fits into the buffer, and / or do a wraparound with partial writes
                throw runtime_error{"Can't cope with wraparound yet"};
            } else { // Nice linear memory
                uint8_t *begin = (uint8_t *) localBuffer;
                begin += beginPos;
                //uint8_t* end = (uint8_t*) localBuffer;
                //end += endPos;
                memcpy(begin, DATA, sizeToWrite);
                MemoryRegion sendBuffer(begin, sizeToWrite, network.getProtectionDomain(),
                                        MemoryRegion::Permission::All);
                RemoteMemoryRegion receiveBuffer;
                receiveBuffer.key = remoteBuffer.key;
                receiveBuffer.address = remoteBuffer.address + sizeToWrite;

                WriteWorkRequest writeRequest;
                writeRequest.setLocalAddress(sendBuffer);
                writeRequest.setRemoteAddress(receiveBuffer);
                writeRequest.setCompletion(false);
                // Dont post yet, we can chain the WRs

                AtomicFetchAndAddWorkRequest atomicAddRequest;
                atomicAddRequest.setLocalAddress(sharedWritePos);
                atomicAddRequest.setAddValue(sizeToWrite);
                atomicAddRequest.setRemoteAddress(remoteWritePos);
                atomicAddRequest.setCompletion(false);

                // We probably don't need an explicit wraparound. The uint64_t will overflow, when we write more than
                // 16384 Petabyte == 16 Exabyte. That probably "ought to be enough for anybody" in the near future.
                // Assume we have 100Gb/s transfer, then we'll overflow in (16*1024*1024*1024/(100/8))s == 700 years
                writePos += sizeToWrite;

                writeRequest.setNextWorkRequest(&atomicAddRequest);

                queuePair.postWorkRequest(writeRequest); // Only one post for better performance
                cout << "Posted write. Current Pos: " << writePos << endl;
            }
        }
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

        const size_t completeBufferSize = BUFFER_SIZE;
        uint8_t localBuffer[completeBufferSize]{};
        MemoryRegion sharedMR(localBuffer, BUFFER_SIZE, network.getProtectionDomain(), MemoryRegion::Permission::All);
        uint64_t readPosition = 0;
        volatile uint64_t sharedBufferWritePosition = 0;
        MemoryRegion sharedWritePos((void *) &sharedBufferWritePosition, sizeof(uint64_t),
                                    network.getProtectionDomain(), MemoryRegion::Permission::All);
        MemoryRegion sharedReadPos(&readPosition, sizeof(readPosition), network.getProtectionDomain(),
                                   MemoryRegion::Permission::All);
        // TODO: share readPos
        sendRmrInfo(acced, sharedMR, sharedWritePos);

        for (int i = 0; i < 4; ++i) {
            const size_t sizeToRead = sizeof(DATA);
            size_t beginPos = readPosition;
            size_t endPos = (readPosition + sizeToRead) % completeBufferSize;
            if (endPos < beginPos) {
                // TODO: split and wraparound
                // TODO: check if our read still fits into the buffer, and / or do a wraparound with partial reads
                throw runtime_error{"Can't cope with wraparound yet"};
            } else { // Nice linear data
                // Spin wait until we have some data
                while (readPosition == sharedBufferWritePosition) sched_yield();
                const size_t begin = readPosition % completeBufferSize;
                readPosition += sizeToRead;

                for (size_t j = 0; j < sizeToRead; ++j) {
                    cout << localBuffer[begin + j];
                }
                cout << endl;
            }
        }

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

