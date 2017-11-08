#include "RDMAMessageBuffer.h"
#include <iostream>
#include <infiniband/verbs.h>
#include "exchangeableTransports/rdma/WorkRequest.hpp"
#include "exchangeableTransports/util/tcpWrapper.h"

using namespace std;
using namespace rdma;

static const size_t validity = 0xDEADDEADBEEFBEEF;

struct RmrInfo {
    uint32_t bufferKey;
    uint32_t readPosKey;
    uintptr_t bufferAddress;
    uintptr_t readPosAddress;
};

static void receiveAndSetupRmr(int sock, RemoteMemoryRegion &buffer, RemoteMemoryRegion &readPos) {
    RmrInfo rmrInfo{};
    tcp_read(sock, &rmrInfo, sizeof(rmrInfo));
    buffer.key = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
    readPos.key = rmrInfo.readPosKey;
    readPos.address = rmrInfo.readPosAddress;
}

static void sendRmrInfo(int sock, const MemoryRegion &buffer, const MemoryRegion &readPos) {
    RmrInfo rmrInfo{};
    rmrInfo.bufferKey = buffer.key->rkey;
    rmrInfo.bufferAddress = reinterpret_cast<uintptr_t>(buffer.address);
    rmrInfo.readPosKey = readPos.key->rkey;
    rmrInfo.readPosAddress = reinterpret_cast<uintptr_t>(readPos.address);
    tcp_write(sock, &rmrInfo, sizeof(rmrInfo));
}

static void exchangeQPNAndConnect(int sock, Network &network, QueuePair &queuePair) {
    Address addr{};
    addr.lid = network.getLID();
    addr.qpn = queuePair.getQPN();
    tcp_write(sock, &addr, sizeof(addr)); // Send own qpn to server
    tcp_read(sock, &addr, sizeof(addr)); // receive qpn
    queuePair.connect(addr);
    cout << "connected to qpn " << addr.qpn << " lid: " << addr.lid << endl;
}

vector<uint8_t> RDMAMessageBuffer::receive() {
    size_t receiveSize = 0;
    auto receiveValidity = static_cast<decltype(validity)>(0);
    do {
        readFromReceiveBuffer(readPos, reinterpret_cast<uint8_t *>(&receiveSize), sizeof(receiveSize));
        readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize,
                              reinterpret_cast<uint8_t *>(&receiveValidity), sizeof(receiveValidity));
    } while (receiveValidity != validity);

    auto result = vector<uint8_t>(receiveSize);
    readFromReceiveBuffer(readPos + sizeof(receiveSize), result.data(), receiveSize);
    zeroReceiveBuffer(readPos, sizeof(receiveSize) + receiveSize + sizeof(validity));

    readPos += sizeof(receiveSize) + receiveSize + sizeof(validity);

    return result;
}

size_t RDMAMessageBuffer::receive(void *whereTo, size_t maxSize) {
    size_t receiveSize = 0;
    auto receiveValidity = static_cast<decltype(validity)>(0);
    do {
        readFromReceiveBuffer(readPos, reinterpret_cast<uint8_t *>(&receiveSize), sizeof(receiveSize));
        readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize,
                              reinterpret_cast<uint8_t *>(&receiveValidity),
                              sizeof(receiveValidity));
    } while (receiveValidity != validity);

    if (receiveSize > maxSize) {
        throw runtime_error{"plz only read whole messages for now!"}; // probably buffer partially read msgs
    }
    readFromReceiveBuffer(readPos + sizeof(receiveSize), reinterpret_cast<uint8_t *>(whereTo), receiveSize);
    zeroReceiveBuffer(readPos, sizeof(receiveSize) + receiveSize + sizeof(validity));

    readPos += sizeof(receiveSize) + receiveSize + sizeof(validity);

    return receiveSize;
}

RDMAMessageBuffer::RDMAMessageBuffer(size_t size, int sock) :
        size(size),
        net(sock),
        receiveBuffer(make_unique<volatile uint8_t[]>(size)),
        sendBuffer(make_unique<uint8_t[]>(size)),
        localSend(sendBuffer.get(), size, net.network.getProtectionDomain(), MemoryRegion::Permission::None),
        localReceive(const_cast<uint8_t *>(receiveBuffer.get()), size, net.network.getProtectionDomain(),
                     MemoryRegion::Permission::LocalWrite | MemoryRegion::Permission::RemoteWrite),
        localReadPos(&readPos, sizeof(readPos), net.network.getProtectionDomain(),
                     MemoryRegion::Permission::RemoteRead),
        localCurrentRemoteReceive(const_cast<size_t *>(&currentRemoteReceive), sizeof(currentRemoteReceive),
                                  net.network.getProtectionDomain(), MemoryRegion::Permission::LocalWrite) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw runtime_error{"size should be a power of 2"};
    }

    tcp_setBlocking(sock); // just set the socket to block for our setup.

    sendRmrInfo(sock, localReceive, localReadPos);
    receiveAndSetupRmr(sock, remoteReceive, remoteReadPos);
}

/// Higher order wraparound function. Calls the given function func() once or twice, depending on if a wraparound is needed or not
template<typename Func>
void wraparound(const size_t totalSize, const size_t todoSize, const size_t pos, Func &&func) {
    const size_t beginPos = pos & (totalSize - 1);
    if ((totalSize - beginPos) >= todoSize) {
        func(0, beginPos, beginPos + todoSize);
    } else {
        const auto fst = beginPos;
        const auto fstToRead = totalSize - beginPos;
        const auto snd = 0;
        const auto sndToRead = todoSize - fstToRead;
        func(0, fst, fst + fstToRead);
        func(fstToRead, snd, snd + sndToRead);
    }
}

/// func(size_t prevBytes, T* begin, T* end)
template<typename T, typename Func>
void wraparound(T *buffer, const size_t totalSize, const size_t todoSize, const size_t pos, Func &&func) {
    wraparound(totalSize, todoSize, pos, [&](auto prevBytes, auto beginPos, auto endPos) {
        func(prevBytes, buffer + beginPos, buffer + endPos);
    });
}

void RDMAMessageBuffer::send(const uint8_t *data, size_t length) {
    send(data, length, true);
}

void RDMAMessageBuffer::send(const uint8_t *data, size_t length, bool inln) {
    const size_t sizeToWrite = sizeof(length) + length + sizeof(validity);
    if (sizeToWrite > size) throw runtime_error{"data > buffersize!"};

    const size_t startOfWrite = sendPos;

    writeToSendBuffer(reinterpret_cast<const uint8_t *>(&length), sizeof(length));
    writeToSendBuffer(data, length);
    writeToSendBuffer(reinterpret_cast<const uint8_t *>(&validity), sizeof(validity));

    wraparound(size, sizeToWrite, startOfWrite, [&](auto, auto beginPos, auto endPos) {
        const auto sendSlice = localSend.slice(beginPos, endPos - beginPos);
        const auto remoteSlice = remoteReceive.slice(beginPos);
        WriteWorkRequestBuilder(sendSlice, remoteSlice, false)
                .setInline(inln && sendSlice.size <= net.queuePair.getMaxInlineSize())
                .send(net.queuePair);
    });
}

void RDMAMessageBuffer::writeToSendBuffer(const uint8_t *data, size_t sizeToWrite) {
    // Make sure, there is enough space
    size_t safeToWrite = size - (sendPos - currentRemoteReceive);
    while (sizeToWrite > safeToWrite) {
        ReadWorkRequestBuilder(localCurrentRemoteReceive, remoteReadPos, true)
                .send(net.queuePair);
        while (net.completionQueue.pollSendCompletionQueue() !=
               ReadWorkRequest::getId()); // Poll until read has finished
        safeToWrite = size - (sendPos - currentRemoteReceive);
    }

    wraparound(sendBuffer.get(), size, sizeToWrite, sendPos, [&](auto prevBytes, auto begin, auto end) {
        copy(data + prevBytes, data + prevBytes + distance(begin, end), begin);
    });

    sendPos += sizeToWrite;
}

void RDMAMessageBuffer::readFromReceiveBuffer(size_t readPos, uint8_t *whereTo, size_t sizeToRead) const {
    wraparound(receiveBuffer.get(), size, sizeToRead, readPos, [whereTo](auto prevBytes, auto begin, auto end) {
        copy(begin, end, whereTo + prevBytes);
    });
    // Don't increment currentRead, we might need to read the same position multiple times!
}

void RDMAMessageBuffer::zeroReceiveBuffer(size_t beginReceiveCount, size_t sizeToZero) {
    wraparound(receiveBuffer.get(), size, sizeToZero, beginReceiveCount, [](auto, auto begin, auto end) {
        fill(begin, end, 0);
    });
}

bool RDMAMessageBuffer::hasData() const {
    size_t receiveSize;
    auto receiveValidity = static_cast<decltype(validity)>(0);
    readFromReceiveBuffer(readPos, reinterpret_cast<uint8_t *>(&receiveSize), sizeof(receiveSize));
    readFromReceiveBuffer(readPos + sizeof(receiveSize) + receiveSize, reinterpret_cast<uint8_t *>(&receiveValidity),
                          sizeof(receiveValidity));
    return (receiveValidity == validity);
}

RDMANetworking::RDMANetworking(int sock) :
        completionQueue(network),
        queuePair(network, completionQueue) {
    tcp_setBlocking(sock); // just set the socket to block for our setup.
    exchangeQPNAndConnect(sock, network, queuePair);
}
