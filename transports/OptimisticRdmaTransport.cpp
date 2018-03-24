#include <netinet/in.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <arpa/inet.h>
#include "OptimisticRdmaTransport.h"

static auto uuidGenerator = boost::uuids::random_generator{};

OptimisticRdmaTransportServer::OptimisticRdmaTransportServer(std::string_view port) :
        sock(tcp_socket()), net(), sharedCq(net.getSharedCompletionQueue()) {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);

    receiveBuf = mmapSharedRingBuffer(to_string(uuidGenerator()), receiveBufferSize, true);
    localReceiveMr = net.registerMr(receiveBuf.get(), receiveBufferSize,
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    sendBuf = std::vector<uint8_t>(512);
    localSendBufMr = net.registerMr(sendBuf.data(), sendBuf.size(), {});

    std::fill(doorBells.begin(), doorBells.end(), -1);
    doorBellMr = net.registerMr(doorBells.data(), doorBells.size() * sizeof(int32_t),
                                {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
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
    auto pos = static_cast<int>(connections.size());

    auto ignored = sockaddr_in{};
    auto acced = tcp_accept(sock, ignored);

    // Connect Queue Pairs
    auto connection = IncomingConnection(rdma::RcQueuePair(net));
    connection.doorBellPos = pos;
    connection.tcpSocket = acced;
    auto address = rdma::Address{connection.qp.getQPN(), net.getLID()};
    tcp_write(connection.tcpSocket, address);
    tcp_read(connection.tcpSocket, address);

    // Exchange information of the huge buffer and where the answer should go
    auto remoteMr = ibv::memoryregion::RemoteAddress{
            reinterpret_cast<uintptr_t>(receiveBuf.get()),
            localReceiveMr->getRkey()
    };
    tcp_write(connection.tcpSocket, remoteMr);
    tcp_read(connection.tcpSocket, remoteMr);

    const auto doorBell = ibv::memoryregion::RemoteAddress{
            reinterpret_cast<uintptr_t>(&doorBells[connection.doorBellPos]),
            doorBellMr->getRkey()
    };
    tcp_write(connection.tcpSocket, doorBell);

    connection.qp.connect(address);

    connection.answerWr = ibv::workrequest::Simple<ibv::workrequest::Write>();
    connection.answerWr.setLocalAddress(localSendBufMr->getSlice());
    connection.answerWr.setInline();
    connection.answerWr.setSignaled();

    connections.push_back(std::move(connection));
}

OptimisticRdmaTransportServer::~OptimisticRdmaTransportServer() {
    for (const auto &accepted : connections) {
        tcp_close(accepted.tcpSocket);
    }
}

size_t OptimisticRdmaTransportServer::receive(void *whereTo, size_t maxSize) {
    size_t idOfSender = 0;
    int32_t writePos = -1;
    while (writePos == -1) { // TODO: vectorize: https://godbolt.org/g/jgiU74
        for (size_t i = 0; i < doorBells.size(); ++i) {
            auto data = *reinterpret_cast<volatile int32_t *>(&doorBells[i]);
            idOfSender = i;
            writePos = data;
        }
    }
    doorBells[idOfSender] = -1;

    // write info only contains the write offset
    auto start = receiveBuf.get() + writePos;
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
    // TODO: message verification: Sender + CRC

    return idOfSender;
}

void OptimisticRdmaTransportServer::send(size_t receiverId, const uint8_t *data, size_t size) {
    if (size > sendBuf.size()) {
        throw "can't send message > sendBuf.size()";
    }
    auto whereToWrite = 0;
    const auto write = [&](auto what, auto howManyBytes) {
        auto whatPtr = reinterpret_cast<const uint8_t *>(what);
        volatile auto dest = &sendBuf.data()[whereToWrite];
        std::copy(whatPtr, &whatPtr[howManyBytes], dest);
        whereToWrite += howManyBytes;
    };

    write(size, sizeof(size));
    write(data, size);
    // TODO: validity
    auto &connection = connections[receiverId];
    connection.qp.postWorkRequest(connection.answerWr);
    sharedCq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

OptimisticRdmaTransportClient::OptimisticRdmaTransportClient() :
        sock(tcp_socket()), net(), sharedCq(net.getSharedCompletionQueue()), qp(net) {
    receiveBuf = std::vector<uint8_t>(size);
    localReceiveMr = net.registerMr(receiveBuf.data(), size,
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    sendBuf = std::vector<uint8_t>(size);
    localSendMr = net.registerMr(sendBuf.data(), size, {});
    dataWr.setLocalAddress(localSendMr->getSlice());
    dataWr.setSignaled();
    dataWr.setInline();

    writePos = 0;
    writePosMr = net.registerMr(&writePos, sizeof(writePos), {});
    doorBellWr.setLocalAddress(writePosMr->getSlice());
    doorBellWr.setSignaled();
    doorBellWr.setInline();
}

void OptimisticRdmaTransportClient::connect_impl(std::string_view whereTo) {
    const auto pos = whereTo.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto ip = std::string(whereTo.data(), pos);
    const auto port = std::stoi(std::string(whereTo.begin() + pos + 1, whereTo.end()));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    tcp_connect(sock, addr);

    auto address = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(sock, address);
    tcp_read(sock, address);

    auto remoteMr = ibv::memoryregion::RemoteAddress{
            reinterpret_cast<uintptr_t>(receiveBuf.data()),
            localReceiveMr->getRkey()
    };
    tcp_write(sock, remoteMr);
    tcp_read(sock, remoteMr);
    bigBuffer = remoteMr;

    // read doorbell position
    ibv::memoryregion::RemoteAddress doorBellPos{};
    tcp_read(sock, doorBellPos);
    doorBellWr.setRemoteAddress(doorBellPos);

    qp.connect(address);
}

void OptimisticRdmaTransportClient::send_impl(const uint8_t *data, size_t size) {
    if (size > this->size) {
        throw "no more than 512B fit into inline buffers";
    }

    auto whereToWrite = 0;
    const auto write = [&](auto what, auto howManyBytes) {
        auto whatPtr = reinterpret_cast<const uint8_t *>(what);
        volatile auto dest = &sendBuf.data()[whereToWrite];
        std::copy(whatPtr, &whatPtr[howManyBytes], dest);
        whereToWrite += howManyBytes;
    };

    write(size, sizeof(size));
    write(data, size);
    // TODO: message verification: sender/crc

    writePos = randomDistribution(generator); // set data for doorBellWr TODO: volatile?
    dataWr.setRemoteAddress(bigBuffer.offset(writePos));

    // first! write data, then signal it written
    qp.postWorkRequest(dataWr);
    qp.postWorkRequest(doorBellWr);

    sharedCq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    sharedCq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

size_t OptimisticRdmaTransportClient::receive_impl(void *whereTo, size_t maxSize) {
    size_t receiveSize;
    size_t checkMe;
    size_t validity = 0; // TODO
    do {
        receiveSize = *reinterpret_cast<volatile size_t *>(receiveBuf.data());
        checkMe = *reinterpret_cast<volatile size_t *>(&receiveBuf.data()[sizeof(size_t) + receiveSize]);
    } while (checkMe != validity);

    std::copy(&receiveBuf.data()[sizeof(size_t)],
              &receiveBuf.data()[sizeof(size_t) + receiveSize],
              reinterpret_cast<uint8_t *>(whereTo));
}
