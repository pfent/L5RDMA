#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <random>
#include "util/socket/tcp.h"
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/RcQueuePair.h"
#include "util/bench.h"
#include "util/socket/Socket.h"

using namespace std;
using namespace l5::util;

constexpr uint16_t port = 1234;
const char *ip = "127.0.0.1";
constexpr size_t MESSAGES = 1024 * 1024;
constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB

void connectSocket(const Socket &socket) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    for (int i = 0;; ++i) {
        try {
            tcp::connect(socket, addr);
            break;
        } catch (...) {
            std::this_thread::sleep_for(20ms);
            if (i > 10) throw;
        }
    }
}

void setUpListenSocket(const Socket &socket) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp::bind(socket, addr);
    tcp::listen(socket);
}

auto createWriteWrWithImm(const ibv::memoryregion::Slice &slice) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(slice);
    write.setInline();
    write.setSignaled();
    return write;
}

template<class QueuePair>
void runImmData(bool isClient, uint32_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    const auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice());

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                write.setRemoteAddress(remoteMr.offset(destPos));
                write.setImmData(destPos);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                auto wc = cq.pollRecvWorkCompletionBlocking();
                auto recvPos = wc.getImmData();

                qp.postRecvRequest(recv);

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw;
                }
            }
        });

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice());

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for message being written
                auto wc = cq.pollRecvWorkCompletionBlocking();
                qp.postRecvRequest(recv);

                auto recvPos = wc.getImmData();

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                auto destPos = randomDistribution(generator);
                write.setRemoteAddress(remoteMr.offset(destPos));
                write.setImmData(destPos);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        });
    }
}

auto createWriteWr(const ibv::memoryregion::Slice &slice) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::Write>{};
    write.setLocalAddress(slice);
    write.setInline();
    write.setSignaled();
    return write;
}

template<class QueuePair>
void runChainedWrs(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvPosBuf = std::vector<int32_t>(1);
    auto recvPosMr = net.registerMr(recvPosBuf.data(), recvPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto sendPosBuf = std::vector<int32_t>(1);
    auto sendPosMr = net.registerMr(sendPosBuf.data(), sendPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(socket, &remotePosMr, sizeof(remotePosMr));
        tcp::read(socket, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);
        write.setNext(&posWrite);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));

                qp.postWorkRequest(write); // transitively also posts posWrite
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw;
                }
            }
        });

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(acced, &remotePosMr, sizeof(remotePosMr));
        tcp::read(acced, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);
        write.setNext(&posWrite);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));
                qp.postWorkRequest(write); // transitively also posts posWrite
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        });
    }
}

template<class QueuePair>
void runPostedWrs(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvPosBuf = std::vector<int32_t>(1);
    auto recvPosMr = net.registerMr(recvPosBuf.data(), recvPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto sendPosBuf = std::vector<int32_t>(1);
    auto sendPosMr = net.registerMr(sendPosBuf.data(), sendPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(socket, &remotePosMr, sizeof(remotePosMr));
        tcp::read(socket, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));

                qp.postWorkRequest(write);
                qp.postWorkRequest(posWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw;
                }
            }
        });

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(acced, &remotePosMr, sizeof(remotePosMr));
        tcp::read(acced, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));
                qp.postWorkRequest(write);
                qp.postWorkRequest(posWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        });
    }
}

template<class QueuePair>
void runDoublePollingWrs(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvPosBuf = std::vector<int32_t>(1);
    auto recvPosMr = net.registerMr(recvPosBuf.data(), recvPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto sendPosBuf = std::vector<int32_t>(1);
    auto sendPosMr = net.registerMr(sendPosBuf.data(), sendPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(socket, &remotePosMr, sizeof(remotePosMr));
        tcp::read(socket, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));

                qp.postWorkRequest(posWrite);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                while (*static_cast<volatile char *>(begin.base()) == '\0');
                while (*static_cast<volatile char *>(end.base() - 1) == '\0');
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw;
                }
                std::fill(begin, end, '\0');
            }
        });

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        // invalidate recv address
        recvPosBuf[0] = -1;

        auto remoteAddr = rdma::Address{net.getGID(), qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));
        auto remotePosMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvPosBuf.data()), recvPosMr->getRkey()};
        tcp::write(acced, &remotePosMr, sizeof(remotePosMr));
        tcp::read(acced, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for incoming message
                while (*static_cast<volatile int32_t *>(&recvPosBuf[0]) == -1);
                auto recvPos = recvPosBuf[0];
                recvPosBuf[0] = -1;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                while (*static_cast<volatile char *>(begin.base()) == '\0');
                while (*static_cast<volatile char *>(end.base() - 1) == '\0');
                std::copy(begin, end, sendbuf.begin());
                std::fill(begin, end, '\0');
                // echo back the received data
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));
                qp.postWorkRequest(posWrite);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    cout << "size, connection, messages, seconds, msgps, user, kernel, total" << '\n';
    for (const size_t length : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u}) {
        cout << length << ", ImmPos, ";
        runImmData<rdma::RcQueuePair>(isClient, length);
        cout << length << ", 2WrChained, ";
        runChainedWrs<rdma::RcQueuePair>(isClient, length);
        cout << length << ", 2WrSeparate, ";
        runPostedWrs<rdma::RcQueuePair>(isClient, length);
        cout << length << ", 2WrDoublePoll, ";
        runDoublePollingWrs<rdma::RcQueuePair>(isClient, length);
    }
}
