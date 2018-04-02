#include <util/tcpWrapper.h>
#include <netinet/in.h>
#include <boost/assert.hpp>
#include <emmintrin.h>
#include "MulticlientTransport.h"

static constexpr char validity = '\4'; // ASCII EOT char

MulticlientTransportServer::MulticlientTransportServer(std::string_view port)
        : listenSock(tcp_socket()),
          net(rdma::Network()),
          sharedCq(net.getSharedCompletionQueue()),
          receives(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          doorBells(MAX_CLIENTS, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          sendBuffer(MAX_MESSAGESIZE, net, {}) {
    std::fill(doorBells.begin(), doorBells.end(), '\0');
    listen(std::stoi(std::string(port.data(), port.size())));
}

void MulticlientTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(listenSock, addr);
    tcp_listen(listenSock);
}

void MulticlientTransportServer::accept() {
    const auto clientId = connections.size();

    auto ignored = sockaddr_in{};
    auto acced = tcp_accept(listenSock, ignored);

    auto qp = rdma::RcQueuePair(net);

    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(acced, address);
    tcp_read(acced, address);

    auto receiveAddr = receives.getAddr().offset(sizeof(uint8_t[MAX_MESSAGESIZE]) * clientId);
    tcp_write(acced, receiveAddr);
    tcp_read(acced, receiveAddr);

    auto doorBellAddr = doorBells.getAddr().offset(sizeof(char) * clientId);
    tcp_write(acced, doorBellAddr);

    qp.connect(address);

    auto answer = ibv::workrequest::Simple<ibv::workrequest::Write>();
    answer.setLocalAddress(sendBuffer.getSlice());
    answer.setRemoteAddress(receiveAddr);
    answer.setInline();
    answer.setSignaled();

    connections.push_back(Connection{acced, std::move(qp), answer});
}

MulticlientTransportServer::~MulticlientTransportServer() {
    for (const auto &conn : connections) {
        if (conn.socket != -1) {
            tcp_close(conn.socket);
        }
    }
    tcp_close(listenSock);
}

__always_inline
static size_t poll(char *doorBells, size_t count) {
    for (;;) {
        for (size_t i = 0; i < count; ++i) {
            if (*reinterpret_cast<volatile char *>(&doorBells[i]) != '\0') {
                doorBells[i] = '\0';
                return i;
            }
        }
    }
}

__always_inline
static size_t pollSSE(char *doorBells, size_t count) {
    assert(count % 16 == 0);
    const auto zero = _mm_set1_epi8('\0');
    for (;;) {
        for (size_t i = 0; i < count; i += 16) {
            auto data = *reinterpret_cast<volatile __m128i *>(&doorBells[i]);
            auto cmp = _mm_cmpeq_epi8(zero, data);
            uint16_t cmpMask = compl _mm_movemask_epi8(cmp);
            if (cmpMask != 0) {
                auto lzcnt = __builtin_clz(cmpMask);
                auto sender = 32 - (lzcnt + 1) + i;
                doorBells[sender] = '\0';
                return sender;
            }
        }
    }
}

size_t MulticlientTransportServer::receive(void *whereTo, size_t maxSize) {
    const auto sender = poll(doorBells.data(), MAX_CLIENTS);

    const auto sizePtr = reinterpret_cast<uint8_t *>(receives.data()[sender]);
    const auto size = *reinterpret_cast<size_t *>(sizePtr);
    if (maxSize < size) {
        throw std::runtime_error("received message > maxSize");
    }

    const auto begin = sizePtr + sizeof(size_t);
    const auto end = begin + size;
    std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));

    return sender;
}

void MulticlientTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    const auto totalLength = size + sizeof(size_t) + sizeof(validity);
    if (totalLength > MAX_MESSAGESIZE) {
        throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
    }
    if (receiverId > connections.size()) {
        throw std::runtime_error("no such connection");
    }

    auto &con = connections[receiverId];

    std::copy(&size, &size + 1, reinterpret_cast<size_t *>(sendBuffer.data()));
    std::copy(data, data + size, sendBuffer.data() + sizeof(size_t));
    std::copy(&validity, &validity + 1, sendBuffer.data() + sizeof(size_t) + size);

    con.answerWr.setLocalAddress(sendBuffer.getSlice(0, totalLength));
    sendCounter++;
    if (sendCounter % 1024 == 0) { // selective signaling
        con.answerWr.setFlags({ibv::workrequest::Flags::INLINE, ibv::workrequest::Flags::SIGNALED});
        con.qp.postWorkRequest(con.answerWr);
        sharedCq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    } else {
        con.answerWr.setFlags({ibv::workrequest::Flags::INLINE});
        con.qp.postWorkRequest(con.answerWr);
    }
}

MultiClientTransportClient::MultiClientTransportClient()
        : sock(tcp_socket()),
          net(rdma::Network()),
          cq(net.getSharedCompletionQueue()),
          qp(rdma::RcQueuePair(net)),
          sendBuffer(MAX_MESSAGESIZE, net, {}),
          doorBell(1, net, {}),
          receiveBuffer(MAX_MESSAGESIZE, net, {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE}),
          dataWr(),
          doorBellWr() {
    dataWr.setLocalAddress(sendBuffer.getSlice());
    dataWr.setSignaled();
    dataWr.setInline();

    doorBellWr.setLocalAddress(doorBell.getSlice());
    doorBellWr.setSignaled();
    doorBellWr.setInline();
}

void MultiClientTransportClient::rdmaConnect() {
    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(sock, address);
    tcp_read(sock, address);

    auto receiveAddr = receiveBuffer.getAddr();
    tcp_write(sock, receiveAddr);
    tcp_read(sock, receiveAddr);

    auto doorBellAddr = ibv::memoryregion::RemoteAddress();
    tcp_read(sock, doorBellAddr);

    qp.connect(address);

    dataWr.setRemoteAddress(receiveAddr);
    doorBellWr.setRemoteAddress(doorBellAddr);
}

void MultiClientTransportClient::connect(std::string_view whereTo) {
    tcp_connect(sock, whereTo);

    rdmaConnect();
}

void MultiClientTransportClient::connect(std::string_view ip, uint16_t port) {
    tcp_connect(sock, std::string(ip), port);

    rdmaConnect();
}


void MultiClientTransportClient::send(const uint8_t *data, size_t size) {
    const auto dataWrSize = size + sizeof(size_t);
    if (dataWrSize > MAX_MESSAGESIZE) {
        throw std::runtime_error("can't send messages > MAX_MESSAGESIZE");
    }

    std::copy(&size, &size + 1, reinterpret_cast<size_t *>(sendBuffer.data()));
    std::copy(data, data + size, sendBuffer.data() + sizeof(size_t));
    dataWr.setLocalAddress(sendBuffer.getSlice(0, dataWrSize));
    qp.postWorkRequest(dataWr);

    doorBell.data()[0] = 'X'; // could be anything, really
    qp.postWorkRequest(doorBellWr);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

size_t MultiClientTransportClient::receive(void *whereTo, size_t maxSize) {
    // wait for message being written
    size_t size;
    while ((size = *reinterpret_cast<volatile size_t *>(receiveBuffer.data())) == 0);
    while (*reinterpret_cast<volatile char *>(receiveBuffer.data() + sizeof(size_t) + size) != validity);

    if (size > maxSize) {
        throw std::runtime_error("received message > maxSize");
    }

    const auto begin = receiveBuffer.data() + sizeof(size_t);
    const auto end = begin + size;

    std::copy(begin, end, reinterpret_cast<uint8_t *>(whereTo));
    *reinterpret_cast<volatile size_t *>(receiveBuffer.data()) = 0;

    return size;
}
