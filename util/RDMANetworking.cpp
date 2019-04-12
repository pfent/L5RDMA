#include "RDMANetworking.h"
#include <iostream>
#include "util/socket/tcp.h"

namespace l5 {
namespace util {
static void exchangeQPNAndConnect(const Socket &sock, rdma::Network &network, rdma::QueuePair &queuePair) {
    rdma::Address addr{};
    addr.gid = network.getGID();
    addr.lid = network.getLID();
    addr.qpn = queuePair.getQPN();
    tcp::write(sock, &addr, sizeof(addr)); // Send own qpn to server
    tcp::read(sock, &addr, sizeof(addr)); // receive qpn
    queuePair.connect(addr);
}

RDMANetworking::RDMANetworking(const Socket &sock) :
        completionQueue(network.newCompletionQueuePair()),
        queuePair(network, completionQueue) {
    tcp::setBlocking(sock); // just set the socket to block for our setup.
    exchangeQPNAndConnect(sock, network, queuePair);
}

void
receiveAndSetupRmr(const Socket &sock, ibv::memoryregion::RemoteAddress &buffer,
                   ibv::memoryregion::RemoteAddress &readPos) {
    RmrInfo rmrInfo{};
    tcp::read(sock, &rmrInfo, sizeof(rmrInfo));
    buffer.rkey = rmrInfo.bufferKey;
    buffer.address = rmrInfo.bufferAddress;
    readPos.rkey = rmrInfo.readPosKey;
    readPos.address = rmrInfo.readPosAddress;
}

void sendRmrInfo(const Socket &sock, const ibv::memoryregion::MemoryRegion &buffer,
                 const ibv::memoryregion::MemoryRegion &readPos) {
    RmrInfo rmrInfo{};
    rmrInfo.bufferKey = buffer.getRkey();
    rmrInfo.bufferAddress = reinterpret_cast<uintptr_t>(buffer.getAddr());
    rmrInfo.readPosKey = readPos.getRkey();
    rmrInfo.readPosAddress = reinterpret_cast<uintptr_t>(readPos.getAddr());
    tcp::write(sock, &rmrInfo, sizeof(rmrInfo));
}
} // namespace util
} // namespace l5
