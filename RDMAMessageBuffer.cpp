#include "RDMAMessageBuffer.h"
#include <iostream>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include "rdma/WorkRequest.hpp"
#include "tcpWrapper.h"

using namespace std;
using namespace rdma;

static const size_t validity = 0xDEADDEADBEEFBEEF;

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
    const auto begin = currentReceive;
    size_t receiveSize;
    do {
        readFromReceiveBuffer((uint8_t *) &receiveSize, sizeof(receiveSize));
    } while (receiveSize == 0);
    currentReceive += sizeof(receiveSize);

    // Read the validity @end of message
    currentReceive += receiveSize;
    auto receiveValidity = (decltype(validity)) 0;
    do {
        readFromReceiveBuffer((uint8_t *) &receiveValidity, sizeof(receiveValidity));
    } while (receiveValidity == 0);

    if (receiveValidity != validity) throw runtime_error{"unexpected validity " + receiveValidity};

    auto result = vector<uint8_t>(receiveSize);
    currentReceive -= receiveSize;
    readFromReceiveBuffer(result.data(), receiveSize);
    currentReceive += receiveSize + sizeof(receiveValidity);

    zeroReceiveBuffer(begin, sizeof(receiveSize) + receiveSize + sizeof(validity));
    currentReceive = 0; // TODO: remove
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

    const size_t beginPos = currentSend % size;
    const size_t endPos = (currentSend + sizeToWrite - 1) % size;

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
        auto sendSlice2 = localSend.slice(0, endPos);
        auto remoteSlice2 = remoteReceive.slice(0);

        WriteWorkRequestBuilder(sendSlice1, remoteSlice1, true)
                .send(net.queuePair);
        WriteWorkRequestBuilder(sendSlice2, remoteSlice2, true)
                .send(net.queuePair);
        net.completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE);
    }
    net.completionQueue.pollSendCompletionQueue(IBV_WC_RDMA_WRITE); // This probably leaves one completion in the CQ
    currentSend = 0; // TODO: remove
}

void RDMAMessageBuffer::writeToSendBuffer(uint8_t *data, size_t sizeToWrite) {
    const size_t safeToWrite = size - (currentSend - currentReceive);
    if (sizeToWrite > safeToWrite) {
        // TODO
        throw runtime_error{"can't sync yet"};
    }
    const size_t beginPos = currentSend % size;
    const size_t endPos = (currentSend + sizeToWrite - 1) % size;
    if (endPos >= beginPos) {
        copy(data, data + sizeToWrite, sendBuffer.get() + beginPos);
    } else {
        auto fst = sendBuffer.get() + beginPos;
        auto fstToWrite = size - beginPos;
        auto snd = sendBuffer.get() + 0;
        auto sndToWrite = sizeToWrite - fstToWrite;
        copy(fst, fst + fstToWrite, data);
        copy(snd, snd + sndToWrite, data + fstToWrite);
    }
    currentSend += sizeToWrite;
}

void RDMAMessageBuffer::readFromReceiveBuffer(uint8_t *whereTo, size_t sizeToRead) {
    const size_t beginPos = currentReceive % size;
    const size_t endPos = (currentReceive + sizeToRead - 1) % size;
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
        auto fst = receiveBuffer.get() + beginReceiveCount;
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
