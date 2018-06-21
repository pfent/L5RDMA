#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libibverbscpp/libibverbscpp.h"
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "util/bench.h"
#include "rdma/RcQueuePair.h"
#include "rdma/UcQueuePair.h"
#include "rdma/UdQueuePair.h"
#include <util/socket/tcp.h>

using namespace std;
using namespace l5::util;

constexpr uint16_t port = 1234;
const char *ip = "127.0.0.1";
const size_t MESSAGES = 1024 * 1024;

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

auto createSendWrConnected(const ibv::memoryregion::Slice &slice) {
    auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
    send.setLocalAddress(slice);
    send.setInline();
    send.setSignaled();
    return send;
}

template<class QueuePair>
void runConnected(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(), {ibv::AccessFlag::LOCAL_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto send = createSendWrConnected(sendmr->getSlice());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(send);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::SEND);

                if (cq.pollRecvCompletionQueueBlocking() != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);

                // check if the data is still the same
                if (not std::equal(recvbuf.begin(), recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto send = createSendWrConnected(sendmr->getSlice());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // receive into buf
                if (cq.pollRecvCompletionQueueBlocking() != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);
                std::copy(recvbuf.begin(), recvbuf.end(), sendbuf.begin());
                // echo back the received data
                qp.postWorkRequest(send);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::SEND);
            }
        }, 1);
    }
}

auto createAddressHandleUnconnected(rdma::Network &net, uint16_t lid) {
    ibv::ah::Attributes ahAttributes{};
    ahAttributes.setIsGlobal(false);
    ahAttributes.setDlid(lid);
    ahAttributes.setSl(0);
    ahAttributes.setSrcPathBits(0);
    ahAttributes.setPortNum(1); // local port

    return net.getProtectionDomain().createAddressHandle(ahAttributes);
}

auto createSendWrUnconnected(const ibv::memoryregion::Slice &slice, ibv::ah::AddressHandle &ah, uint32_t qpn) {
    auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
    send.setLocalAddress(slice);
    send.setUDAddressHandle(ah);
    send.setUDRemoteQueue(qpn, 0x22222222);
    send.setInline();
    send.setSignaled();
    return send;
}

void runUnconnected(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = rdma::UdQueuePair(net);

    // from `man ibv_post_recv`:
    // [for UD:] in all cases, the actual data of the incoming message will start at an offset of 40 bytes into the buffer
    auto recvbuf = std::vector<char>(40 + data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(), {ibv::AccessFlag::LOCAL_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        auto ah = createAddressHandleUnconnected(net, remoteAddr.lid);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto send = createSendWrUnconnected(sendmr->getSlice(), *ah, remoteAddr.qpn);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(send);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::SEND);

                if (cq.pollRecvCompletionQueueBlocking() != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);

                // check if the data is still the same
                if (not std::equal(recvbuf.begin() + 40, recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);
    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        auto ah = createAddressHandleUnconnected(net, remoteAddr.lid);

        auto send = createSendWrUnconnected(sendmr->getSlice(), *ah, remoteAddr.qpn);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // receive into buf
                if (cq.pollRecvCompletionQueueBlocking() != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);
                std::copy(recvbuf.begin() + 40, recvbuf.end(), sendbuf.begin());
                // echo back the received data
                qp.postWorkRequest(send);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::SEND);
            }
        }, 1);
    }
}

auto createWriteWrNoImm(const ibv::memoryregion::Slice &slice, const ibv::memoryregion::RemoteAddress &rmr) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::Write>{};
    write.setLocalAddress(slice);
    write.setRemoteAddress(rmr);
    write.setInline();
    write.setSignaled();
    return write;
}

template<class QueuePair>
void runWriteMemPolling(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for message being written
                while (*static_cast<volatile char *>(recvbuf.data()) == '\0');
                while (*static_cast<volatile char *>(recvbuf.data() + recvbuf.size() - 1) == '\0');

                // check if the data is still the same
                if (not std::equal(recvbuf.begin(), recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for message being written
                while (*static_cast<volatile char *>(recvbuf.data()) == '\0');
                while (*static_cast<volatile char *>(recvbuf.data() + recvbuf.size() - 1) == '\0');

                std::copy(recvbuf.begin(), recvbuf.end(), sendbuf.begin());
                std::fill(recvbuf.begin(), recvbuf.end(), 0);
                // echo back the received data
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        }, 1);
    }
}

auto createWriteWrWithImm(const ibv::memoryregion::Slice &slice, const ibv::memoryregion::RemoteAddress &rmr) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(slice);
    write.setRemoteAddress(rmr);
    write.setInline();
    write.setSignaled();
    return write;
}

template<class QueuePair>
void runWriteWithImm(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) {
        {
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

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                if (cq.pollRecvCompletionQueueBlocking(ibv::workcompletion::Opcode::RECV_RDMA_WITH_IMM) != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);

                // check if the data is still the same
                if (not std::equal(recvbuf.begin(), recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);

    } else {
        {   // setup tcp socket
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            tcp::bind(socket, addr);
            tcp::listen(socket);
        }

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                // wait for message being written
                if (cq.pollRecvCompletionQueueBlocking(ibv::workcompletion::Opcode::RECV_RDMA_WITH_IMM) != 42) {
                    throw;
                }
                qp.postRecvRequest(recv);

                std::copy(recvbuf.begin(), recvbuf.end(), sendbuf.begin());
                std::fill(recvbuf.begin(), recvbuf.end(), 0);
                // echo back the received data
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        }, 1);
    }
}

auto createRead(const ibv::memoryregion::Slice &slice, const ibv::memoryregion::RemoteAddress &rmr) {
    auto read = ibv::workrequest::Simple<ibv::workrequest::Read>{};
    read.setLocalAddress(slice);
    read.setRemoteAddress(rmr);
    read.setSignaled();
    return read;
}

void runRead(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = rdma::RcQueuePair(net);

    auto recvbuf = std::vector<char>(data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE,
                                  ibv::AccessFlag::REMOTE_READ});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) { // client writes to server, then reads the written data back
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);
        auto read = createRead(recvmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(write);
                qp.postWorkRequest(read);

                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_READ);

                // check if the data is still the same
                if (not std::equal(recvbuf.begin(), recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);

        tcp::write(socket, "EOF", 4);
    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        char eof[4];
        tcp::read(acced, &eof, 4);
        if (std::string("EOF") != eof) {
            throw;
        }
    }
}

auto createReadUnsignaled(const ibv::memoryregion::Slice &slice, const ibv::memoryregion::RemoteAddress &rmr) {
    auto read = ibv::workrequest::Simple<ibv::workrequest::Read>{};
    read.setLocalAddress(slice);
    read.setRemoteAddress(rmr);
    return read;
}

void runReadPolling(bool isClient, size_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = rdma::RcQueuePair(net);

    auto recvbuf = std::vector<char>(data.size());
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE,
                                  ibv::AccessFlag::REMOTE_READ});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = Socket::create();
    if (isClient) { // client writes to server, then reads the written data back
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(socket, &remoteMr, sizeof(remoteMr));
        tcp::read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);
        auto read = createReadUnsignaled(recvmr->getSlice(), remoteMr);

        bench(MESSAGES, [&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                std::fill(recvbuf.begin(), recvbuf.end(), 0);

                qp.postWorkRequest(write);
                qp.postWorkRequest(read);

                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                while (*static_cast<volatile char *>(recvbuf.data()) == '\0');
                while (*static_cast<volatile char *>(recvbuf.data() + recvbuf.size() - 1) == '\0');

                // check if the data is still the same
                if (not std::equal(recvbuf.begin(), recvbuf.end(), data.begin(), data.end())) {
                    throw;
                }
            }
        }, 1);

        tcp::write(socket, "EOF", 4);
    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp::accept(socket, ignored);
        }();

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp::write(acced, &remoteMr, sizeof(remoteMr));
        tcp::read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        char eof[4];
        tcp::read(acced, &eof, 4);
        if (std::string("EOF") != eof) {
            throw;
        }
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
        cout << length << ", Send, ";
        runConnected<rdma::RcQueuePair>(isClient, length);
        cout << length << ", Write + Polling, ";
        runWriteMemPolling<rdma::RcQueuePair>(isClient, length);
        cout << length << ", Write + Immediate, ";
        runWriteWithImm<rdma::RcQueuePair>(isClient, length);
        if (isClient)
            cout << length << ", Read + Work Completion, ";
        runRead(isClient, length);
        if (isClient)
            cout << length << ", Read + Polling, ";
        runReadPolling(isClient, length);
    }
}
