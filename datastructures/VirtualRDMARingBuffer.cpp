#include "VirtualRDMARingBuffer.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using Perm = ibv::AccessFlag;

namespace l5 {
namespace datastructure {
static auto uuidGenerator = boost::uuids::random_generator{};
using namespace util;

VirtualRDMARingBuffer::VirtualRDMARingBuffer(size_t size, const Socket &sock) :
        size(size), bitmask(size - 1), net(sock),
        sendBuf(mmapSharedRingBuffer(to_string(uuidGenerator()), size, true)),
        // Since we mapped twice the virtual memory, we can create memory regions of twice the size of the actual buffer
        localSendMr(net.network.registerMr(sendBuf.get(), size * 2, {})),
        localReadPosMr(net.network.registerMr(&localReadPos, sizeof(localReadPos), {Perm::REMOTE_READ})),
        receiveBuf(mmapSharedRingBuffer(to_string(uuidGenerator()), size, true)),
        localReceiveMr(net.network.registerMr(receiveBuf.get(), size * 2, {Perm::LOCAL_WRITE, Perm::REMOTE_WRITE})),
        remoteReadPosMr(net.network.registerMr(&remoteReadPos, sizeof(remoteReadPos), {Perm::LOCAL_WRITE})) {
    const bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (not powerOfTwo) {
        throw std::runtime_error{"size should be a power of 2"};
    }

    sendRmrInfo(sock, *localReceiveMr, *localReadPosMr);
    receiveAndSetupRmr(sock, remoteReceiveRmr, remoteReadPosRmr);
}

void VirtualRDMARingBuffer::send(const uint8_t *data, size_t length) {
    send([&](auto writeBegin) {
        std::copy(data, data + length, writeBegin);
        return length;
    });
}

size_t VirtualRDMARingBuffer::receive(void *whereTo, size_t maxSize) {
    const auto maxSizeToRead = sizeof(maxSize) + maxSize + sizeof(validity);
    if (maxSizeToRead > size) throw std::runtime_error{"receiveSize > buffersize!"};
    size_t receiveSize;

    receive([&](auto begin, auto end) {
        receiveSize = static_cast<size_t>(std::distance(begin, end));
        if (receiveSize > maxSize) {
            throw std::runtime_error{"plz only read whole messages for now!"}; // probably buffer partially read msgs
        }

        std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));
    });

    return receiveSize;
}

void VirtualRDMARingBuffer::waitUntilSendFree(size_t sizeToWrite) {
    // Make sure, there is enough space
    size_t safeToWrite = size - (sendPos - remoteReadPos.load());
    while (sizeToWrite > safeToWrite) {
        ibv::workrequest::Simple<ibv::workrequest::Read> wr;
        wr.setLocalAddress(remoteReadPosMr->getSlice());
        wr.setRemoteAddress(remoteReadPosRmr);
        wr.setFlags({ibv::workrequest::Flags::SIGNALED});
        wr.setId(42);
        net.queuePair.postWorkRequest(wr);

        while (net.completionQueue.pollSendCompletionQueue() != 42); // Poll until read has finished
        // Poll until read has finished
        safeToWrite = size - (sendPos - remoteReadPos.load());
    }
}
} // namespace datastructure
} // namespace l5
