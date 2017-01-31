#include "RDMAMessageBuffer.h"
#include <iostream>
#include <infiniband/verbs.h>
#include "rdma/WorkRequest.hpp"
#include "tcpWrapper.h"

using namespace std;
using namespace rdma;

static const size_t validity = 0xDEADDEADBEEFBEEF;

static void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer, RemoteMemoryRegion &readPos) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
        uint32_t readPosKey;
        uintptr_t readPosAddress;
    } rmrInfo;
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    buffer.key = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
    readPos.key = rmrInfo.readPosKey;
    readPos.address = rmrInfo.readPosAddress;
}

static void sendRmrInfo(int sock, const MemoryRegion &buffer, const MemoryRegion &readPos) {
    struct {
        uint32_t bufferKey;
        uintptr_t bufferAddress;
        uint32_t readPosKey;
        uintptr_t readPosAddress;
    } rmrInfo;
    rmrInfo.bufferKey = buffer.key->rkey;
    rmrInfo.bufferAddress = reinterpret_cast<uintptr_t>(buffer.address);
    rmrInfo.readPosKey = readPos.key->rkey;
    rmrInfo.readPosAddress = reinterpret_cast<uintptr_t>(readPos.address);
    tcp_write(sock, &rmrInfo, sizeof(rmrInfo));
}

static void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair) {
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
    auto receiveValidity = (decltype(validity)) 0;
    do {
        readFromReceiveBuffer(readPos, (uint8_t *) &receiveSize, sizeof(receiveSize));
        readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize, (uint8_t *) &receiveValidity,
                              sizeof(receiveValidity));
    } while (receiveValidity != validity);

    auto result = vector<uint8_t>(receiveSize);
    readFromReceiveBuffer(readPos + sizeof(receiveSize), result.data(), receiveSize);
    zeroReceiveBuffer(readPos, sizeof(receiveSize) + receiveSize + sizeof(validity));

    readPos += sizeof(receiveSize) + receiveSize + sizeof(validity);

    return result;
}

size_t RDMAMessageBuffer::receive(void *whereTo, size_t maxSize) {
    size_t receiveSize;
    auto receiveValidity = (decltype(validity)) 0;
    do {
        readFromReceiveBuffer(readPos, (uint8_t *) &receiveSize, sizeof(receiveSize));
        readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize, (uint8_t *) &receiveValidity,
                              sizeof(receiveValidity));
    } while (receiveValidity != validity);

    if (receiveSize > maxSize)
        throw runtime_error{"plz only read whole messages for now!"}; // probably buffer partially read msgs
    readFromReceiveBuffer(readPos + sizeof(receiveSize), (uint8_t *) whereTo, receiveSize);
    zeroReceiveBuffer(readPos, sizeof(receiveSize) + receiveSize + sizeof(validity));

    readPos += sizeof(receiveSize) + receiveSize + sizeof(validity);

    return receiveSize;
}

RDMAMessageBuffer::RDMAMessageBuffer(size_t size, int sock) :
        size(size),
        bitmask(size - 1),
        net(sock),
        receiveBuffer(make_unique<volatile uint8_t[]>(size)),
        sendBuffer(make_unique<uint8_t[]>(size)),
        localSend(sendBuffer.get(), size, net.network.getProtectionDomain(), MemoryRegion::Permission::None),
        localReceive((void *) receiveBuffer.get(), size, net.network.getProtectionDomain(),
                     MemoryRegion::Permission::LocalWrite | MemoryRegion::Permission::RemoteWrite),
        localReadPos(&readPos, sizeof(readPos), net.network.getProtectionDomain(),
                     MemoryRegion::Permission::RemoteRead),
        localCurrentRemoteReceive((void *) &currentRemoteReceive, sizeof(currentRemoteReceive),
                                  net.network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }

    tcp_setBlocking(sock); // just set the socket to block for our setup.

    sendRmrInfo(sock, localReceive, localReadPos);
    receiveAndSetupRmr(sock, remoteReceive, remoteReadPos);
}

void RDMAMessageBuffer::sendInline(const uint8_t *data, size_t length) {
    size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
    const size_t beginPos = sendPos & bitmask;
    const size_t endPos = (sendPos + sizeToWrite - 1) & bitmask;

    // Don't actually write to a pinned memory region. RDMA doesn't care about the lkey, if IBV_SEND_INLINE is set
    if (endPos >= beginPos) {
        const auto sizeSlice = MemoryRegion::Slice(&sizeToWrite, sizeof(sizeToWrite), 0);
        const auto dataSlice = MemoryRegion::Slice((void *) data, sizeToWrite, 0);
        const auto validitySlice = MemoryRegion::Slice((void *) &validity, sizeof(validity), 0);

        const auto remoteSlice = remoteReceive.slice(beginPos);
        WriteWorkRequest wr;
        wr.setSendInline(true);
        wr.setLocalAddress({sizeSlice, dataSlice, validitySlice});
        wr.setRemoteAddress(remoteSlice);
        wr.setCompletion(true);
        net.queuePair.postWorkRequest(wr);
    } else {
        // TODO
        throw;
        // beginPos ~ buffer end
        const auto sendSlice1 = localSend.slice(beginPos, size - beginPos);
        const auto remoteSlice1 = remoteReceive.slice(beginPos);
        // buffer start ~ endpos
        const auto sendSlice2 = localSend.slice(0, endPos + 1);
        const auto remoteSlice2 = remoteReceive.slice(0);

        WriteWorkRequestBuilder(sendSlice1, remoteSlice1, true)
                .send(net.queuePair);
        WriteWorkRequestBuilder(sendSlice2, remoteSlice2, true)
                .send(net.queuePair);
        net.completionQueue.pollSendCompletionQueue();
    }

    sendPos += sizeToWrite;
    net.completionQueue.pollSendCompletionQueue(); // This probably leaves one completion in the CQ
}

void RDMAMessageBuffer::send(const uint8_t *data, size_t length) {
    const size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};

    const size_t beginPos = sendPos & bitmask;
    const size_t endPos = (sendPos + sizeToWrite - 1) & bitmask;

    /*
    if (sizeToWrite <= net.queuePair.getMaxInlineSize() && endPos >= beginPos) {
        // inlining should be much faster, since we don't have to write to volatile memory
        return sendInline(data, length);
    }
     */

    writeToSendBuffer((uint8_t *) &length, sizeof(length));
    writeToSendBuffer(data, length);
    writeToSendBuffer((uint8_t *) &validity, sizeof(validity));

    if (endPos >= beginPos) {
        const auto sendSlice = localSend.slice(beginPos, sizeToWrite);
        const auto remoteSlice = remoteReceive.slice(beginPos);
        WriteWorkRequestBuilder(sendSlice, remoteSlice, false)
                .send(net.queuePair);
    } else {
        // beginPos ~ buffer end
        const auto sendSlice1 = localSend.slice(beginPos, size - beginPos);
        const auto remoteSlice1 = remoteReceive.slice(beginPos);
        // buffer start ~ endpos
        const auto sendSlice2 = localSend.slice(0, endPos + 1);
        const auto remoteSlice2 = remoteReceive.slice(0);

        WriteWorkRequestBuilder(sendSlice1, remoteSlice1, false)
                .send(net.queuePair);
        WriteWorkRequestBuilder(sendSlice2, remoteSlice2, false)
                .send(net.queuePair);
    }
}

void RDMAMessageBuffer::writeToSendBuffer(const uint8_t *data, size_t sizeToWrite) {
    size_t safeToWrite = size - (sendPos - currentRemoteReceive);
    while (sizeToWrite > safeToWrite) {
        ReadWorkRequestBuilder(localCurrentRemoteReceive, remoteReadPos, true)
                .send(net.queuePair);
        while (net.completionQueue.pollSendCompletionQueue() != 42);
        safeToWrite = size - (sendPos - currentRemoteReceive);
    }
    const size_t beginPos = sendPos & bitmask;
    if ((size - beginPos) > sizeToWrite) {
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
    const size_t beginPos = readPos & bitmask;
    if ((size - beginPos) > sizeToRead) {
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
    const size_t beginPos = beginReceiveCount & bitmask;
    if ((size - beginPos) > sizeToZero) {
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

bool RDMAMessageBuffer::hasData() {
    size_t receiveSize;
    auto receiveValidity = (decltype(validity)) 0;
    readFromReceiveBuffer(readPos, (uint8_t *) &receiveSize, sizeof(receiveSize));
    readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize, (uint8_t *) &receiveValidity,
                          sizeof(receiveValidity));
    return (receiveValidity == validity);
}

RDMANetworking::RDMANetworking(int sock) :
        completionQueue(network),
        queuePair(network, completionQueue) {
    exchangeQPNAndConnect(sock, network, queuePair);
}
