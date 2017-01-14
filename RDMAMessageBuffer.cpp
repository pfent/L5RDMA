#include "RDMAMessageBuffer.h"
#include <iostream>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <iomanip>
#include "rdma/WorkRequest.hpp"
#include "tcpWrapper.h"

using namespace std;
using namespace rdma;

static const size_t validity = 0xDEADDEADBEEFBEEF;

template<typename T>
void dumpBuffer(T begin, T end) {
    for (int i = 0; begin != end; ++begin, ++i) {
        if (i % 8 == 0) {
            cout << "\t";
        }
        if (i % 16 == 0) {
            cout << "\n";
        }
        cout << hex << setw(2) << (int) *begin << " ";
    }
    cout << dec << endl;
}

static inline void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer, RemoteMemoryRegion &readPos) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
        uint32_t readPosKey;
        uintptr_t readPosAddress;
    } rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    buffer.key = ntohl(rmrInfo.bufferKey);
    buffer.address = be64toh(rmrInfo.bufferAddress);
    readPos.key = ntohl(rmrInfo.readPosKey);
    readPos.address = be64toh(rmrInfo.readPosAddress);
}

static inline void sendRmrInfo(int sock, MemoryRegion &buffer, MemoryRegion &readPos) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
        uint32_t readPosKey;
        uintptr_t readPosAddress;
    } rmrInfo;
    rmrInfo.bufferKey = htonl(buffer.key->rkey);
    rmrInfo.bufferAddress = htobe64((uint64_t) buffer.address);
    rmrInfo.readPosKey = htonl(readPos.key->rkey);
    rmrInfo.readPosAddress = htobe64((uint64_t) readPos.address);
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
    size_t receiveSize;
    do {
        readFromReceiveBuffer(readPos, (uint8_t *) &receiveSize, sizeof(receiveSize));
    } while (receiveSize == 0);

    // Read the validity @end of message
    auto receiveValidity = (decltype(validity)) 0;
    do {
        readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize, (uint8_t *) &receiveValidity,
                              sizeof(receiveValidity));
    } while (receiveValidity != validity);
    // TODO: probably also read the size again, since when we get a race, the size may not have been written fully.

    auto result = vector<uint8_t>(receiveSize);
    readFromReceiveBuffer(readPos + sizeof(receiveSize), result.data(), receiveSize);
    zeroReceiveBuffer(readPos, sizeof(receiveSize) + receiveSize + sizeof(validity));

    readPos += sizeof(receiveSize) + receiveSize + sizeof(validity);

    return move(result);
}

RDMAMessageBuffer::RDMAMessageBuffer(size_t size, int sock) :
        size(size),
        net(sock),
        receiveBuffer(new volatile uint8_t[size]{}),
        sendBuffer(new uint8_t[size]{}),
        localSend(sendBuffer.get(), size, net.network.getProtectionDomain(), MemoryRegion::Permission::All),
        localReceive((void *) receiveBuffer.get(), size, net.network.getProtectionDomain(),
                     MemoryRegion::Permission::All),
        localReadPos(&readPos, sizeof(readPos), net.network.getProtectionDomain(), MemoryRegion::Permission::All),
        localCurrentRemoteReceive((void *) &currentRemoteReceive, sizeof(currentRemoteReceive),
                                  net.network.getProtectionDomain(), MemoryRegion::Permission::All) {
    sendRmrInfo(sock, localReceive, localReadPos);
    receiveAndSetupRmr(sock, remoteReceive, remoteReadPos);
}

void RDMAMessageBuffer::send(const uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};

    const size_t beginPos = sendPos % size;
    const size_t endPos = (sendPos + sizeToWrite - 1) % size;

    writeToSendBuffer((uint8_t *) &length, sizeof(length));
    writeToSendBuffer(data, length);
    writeToSendBuffer((uint8_t *) &validity, sizeof(validity));

    if (endPos >= beginPos) {
        auto sendSlice = localSend.slice(beginPos, sizeToWrite);
        auto remoteSlice = remoteReceive.slice(beginPos);
        WriteWorkRequestBuilder(sendSlice, remoteSlice, true)
                .send(net.queuePair);
    } else {
        // beginPos ~ buffer end
        auto sendSlice1 = localSend.slice(beginPos, size - beginPos);
        auto remoteSlice1 = remoteReceive.slice(beginPos);
        // buffer start ~ endpos
        auto sendSlice2 = localSend.slice(0, endPos + 1);
        auto remoteSlice2 = remoteReceive.slice(0);

        WriteWorkRequestBuilder(sendSlice1, remoteSlice1, true)
                .send(net.queuePair);
        WriteWorkRequestBuilder(sendSlice2, remoteSlice2, true)
                .send(net.queuePair);
        net.completionQueue.pollSendCompletionQueue();
    }
    net.completionQueue.pollSendCompletionQueue(); // This probably leaves one completion in the CQ
}

void RDMAMessageBuffer::writeToSendBuffer(const uint8_t *data, size_t sizeToWrite) {
    size_t safeToWrite = size - (sendPos - currentRemoteReceive);
    while (sizeToWrite > safeToWrite) {
        ReadWorkRequestBuilder(localCurrentRemoteReceive, remoteReadPos, true)
                .send(net.queuePair);
        net.completionQueue.pollSendCompletionQueue();
        safeToWrite = size - (sendPos - currentRemoteReceive);
    }
    const size_t beginPos = sendPos % size;
    const size_t endPos = (sendPos + sizeToWrite - 1) % size;
    if (endPos >= beginPos) {
        copy(data, data + sizeToWrite, sendBuffer.get() + beginPos);
    } else {
        auto fst = sendBuffer.get() + beginPos;
        auto fstToWrite = size - beginPos;
        auto snd = sendBuffer.get() + 0;
        copy(data, data + fstToWrite, fst);
        copy(data + fstToWrite, data + sizeToWrite, snd);
    }
    sendPos += sizeToWrite;
}

void RDMAMessageBuffer::readFromReceiveBuffer(size_t readPos, uint8_t *whereTo, size_t sizeToRead) {
    const size_t beginPos = readPos % size;
    const size_t endPos = (readPos + sizeToRead - 1) % size;
    if (endPos >= beginPos) {
        copy(receiveBuffer.get() + beginPos, receiveBuffer.get() + beginPos + sizeToRead, whereTo);
    } else {
        auto fst = receiveBuffer.get() + beginPos;
        auto fstToRead = size - beginPos;
        auto snd = receiveBuffer.get() + 0;
        auto sndToRead = sizeToRead - fstToRead;
        copy(fst, fst + fstToRead, whereTo);
        copy(snd, snd + sndToRead, whereTo + fstToRead);
    }
    // Don't increment currentRead, we might need to read the same position multiple times!
}

void RDMAMessageBuffer::zeroReceiveBuffer(size_t beginReceiveCount, size_t sizeToZero) {
    const size_t beginPos = beginReceiveCount % size;
    const size_t endPos = (beginReceiveCount + sizeToZero - 1) % size;
    if (endPos >= beginPos) {
        fill(receiveBuffer.get() + beginPos, receiveBuffer.get() + beginPos + sizeToZero, 0);
    } else {
        auto fst = receiveBuffer.get() + beginPos;
        auto fstToRead = size - beginPos;
        auto snd = receiveBuffer.get() + 0;
        auto sndToRead = sizeToZero - fstToRead;
        fill(fst, fst + fstToRead, 0);
        fill(snd, snd + sndToRead, 0);
    }
}

RDMANetworking::RDMANetworking(int sock) :
        completionQueue(network),
        queuePair(network, completionQueue) {
    exchangeQPNAndConnect(sock, network, queuePair);
}
