#include <netinet/in.h>
#include "OptimisticRdmaTransport.h"

OptimisticRdmaTransportServer::OptimisticRdmaTransportServer(std::string_view port) :
        sock(tcp_socket()), net() {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);

    // TODO: init WraparoundBuffer
    // TODO: init receiveMr
    // TODO: init localSendBuffer
    // TODO: init sendBuf
}

void OptimisticRdmaTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(sock, addr);
    tcp_listen(sock);
}

void OptimisticRdmaTransportServer::accept_impl() {
    const auto currentIndex = acceptedSockets.size();

    auto ignored = sockaddr_in{};
    acceptedSockets.push_back(tcp_accept(sock, ignored));

    // for each freshly connected remote side, we need a new queue pair
    qps.emplace_back(net, ibv::queuepair::Type::RC);
    auto address = rdma::Address{qps.back().getQPN(), net.getLID()};
    tcp_write(acceptedSockets.back(), &address, sizeof(address));
    tcp_read(acceptedSockets.back(), &address, sizeof(address));

    // TODO: also exchange memory locations / remote memory regions

    qps.back().connect(address);

    recvRequests.emplace_back();
    recvRequests.back().setId(currentIndex);

    // TODO: set up answerWorkRequests

    // post a recv for each connection
    qps.back().postRecvRequest(recvRequests.back());
}

OptimisticRdmaTransportServer::~OptimisticRdmaTransportServer() {
    for (const auto accepted : acceptedSockets) {
        tcp_close(accepted);
    }
}

size_t OptimisticRdmaTransportServer::receive(void *whereTo, size_t maxSize) {
    // TODO: this would probably be easier on the CPU with ibv_req_notify_cq
    auto wc = net.getSharedCompletionQueue().pollRecvWorkCompletionBlocking();
    if (not wc.hasImmData()) {
        throw "unexpected work completion";
    }

    const auto idOfSender = wc.getId();
    const auto writeInfo = wc.getImmData();

    // write info only contains the write offset
    auto start = receiveBuf.get() + writeInfo;
    auto messageSize = *reinterpret_cast<size_t *>(start);
    if (messageSize > receiveBufferSize) {
        throw "can't receive messages > buffersize";
    }
    if (messageSize > maxSize) {
        throw "plz only read whole messages for now!";
    }
    // no need for any validity, since the recv completion means the message has already been written completely
    auto begin = start + sizeof(messageSize);
    auto end = begin + messageSize;
    std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));

    // prepare to receive another message from the same remote
    qps[idOfSender].postRecvRequest(recvRequests[idOfSender]);

    return idOfSender;
}

void OptimisticRdmaTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    if (size > sendBuf.size()) {
        throw "can't send messages > sendBuf.size()";
    }
    // TODO: write size, validity
    std::copy(data, data + size, sendBuf.data());
    auto &wr = answerWorkRequests[receiverId];
    wr.setLocalAddress(localSendBufMr->getSlice(0, size));
    qps[receiverId].postWorkRequest(wr);

    // TODO: selective signaling
}
