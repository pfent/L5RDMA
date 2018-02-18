#include <iostream>
#include <thread>
#include <exchangeableTransports/util/tcpWrapper.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libibverbscpp/libibverbscpp.h>
#include <exchangeableTransports/rdma/Network.hpp>
#include <exchangeableTransports/rdma/QueuePair.hpp>
#include <exchangeableTransports/util/bench.h>
#include <exchangeableTransports/rdma/RcQueuePair.h>
#include <exchangeableTransports/rdma/UcQueuePair.h>
#include <exchangeableTransports/rdma/UdQueuePair.h>

using namespace std;

constexpr uint16_t port = 1234;
constexpr auto ip = "127.0.0.1";
const size_t SHAREDMEM_MESSAGES = 1024 * 256;

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

    auto socket = tcp_socket();
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            for (int i = 0;; ++i) {
                try {
                    tcp_connect(socket, addr);
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(20ms);
                    if (i > 10) throw;
                }
            }
        }

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto send = createSendWrConnected(sendmr->getSlice());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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
        {   // setup tcp socket
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            tcp_bind(socket, addr);
            tcp_listen(socket);
        }

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        auto send = createSendWrConnected(sendmr->getSlice());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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

        tcp_close(acced);
    }

    tcp_close(socket);
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

    auto socket = tcp_socket();
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            for (int i = 0;; ++i) {
                try {
                    tcp_connect(socket, addr);
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(20ms);
                    if (i > 10) throw;
                }
            }
        }

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        auto ah = createAddressHandleUnconnected(net, remoteAddr.lid);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto send = createSendWrUnconnected(sendmr->getSlice(), *ah, remoteAddr.qpn);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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
        {   // setup tcp socket
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            tcp_bind(socket, addr);
            tcp_listen(socket);
        }

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        auto receiveInfo = recvmr->getSlice();
        recv.setSge(&receiveInfo, 1);

        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        auto ah = createAddressHandleUnconnected(net, remoteAddr.lid);

        auto send = createSendWrUnconnected(sendmr->getSlice(), *ah, remoteAddr.qpn);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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

        tcp_close(acced);
    }

    tcp_close(socket);
}

auto createWriteWrNoImm(const ibv::memoryregion::Slice &slice, const rdma::RemoteMemoryRegion &rmr) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::Write>{};
    write.setLocalAddress(slice);
    write.setRemoteAddress(rmr.address, rmr.key);
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

    auto socket = tcp_socket();
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            for (int i = 0;; ++i) {
                try {
                    tcp_connect(socket, addr);
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(20ms);
                    if (i > 10) throw;
                }
            }
        }

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = rdma::RemoteMemoryRegion{reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};
        tcp_write(socket, &remoteMr, sizeof(remoteMr));
        tcp_read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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
        {   // setup tcp socket
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            tcp_bind(socket, addr);
            tcp_listen(socket);
        }

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = rdma::RemoteMemoryRegion{reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};
        tcp_write(acced, &remoteMr, sizeof(remoteMr));
        tcp_read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrNoImm(sendmr->getSlice(), remoteMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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

        tcp_close(acced);
    }

    tcp_close(socket);
}

auto createWriteWrWithImm(const ibv::memoryregion::Slice &slice, const rdma::RemoteMemoryRegion &rmr) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(slice);
    write.setRemoteAddress(rmr.address, rmr.key);
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

    auto socket = tcp_socket();
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            for (int i = 0;; ++i) {
                try {
                    tcp_connect(socket, addr);
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(20ms);
                    if (i > 10) throw;
                }
            }
        }

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42); // TODO: do we need more here?
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = rdma::RemoteMemoryRegion{reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};
        tcp_write(socket, &remoteMr, sizeof(remoteMr));
        tcp_read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice(), remoteMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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

            tcp_bind(socket, addr);
            tcp_listen(socket);
        }

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = rdma::RemoteMemoryRegion{reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};
        tcp_write(acced, &remoteMr, sizeof(remoteMr));
        tcp_read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice(), remoteMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
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

        tcp_close(acced);
    }

    tcp_close(socket);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    cout << "size, connection, messages, seconds, msg/s, user, kernel, total" << '\n';
    for (const size_t length : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512}) {
        /*
        cout << length << ", RC, ";
        runConnected<rdma::RcQueuePair>(isClient, length);
        cout << length << ", UC, ";
        runConnected<rdma::UcQueuePair>(isClient, length);
        cout << length << ", UD, ";
        runUnconnected(isClient, length);
         */
        //cout << length << ", WriteRC, ";
        //runWriteMemPolling<rdma::RcQueuePair>(isClient, length);
        //cout << length << ", WriteUc, "; // TODO: still needs debugging
        //runWriteMemPolling<rdma::UcQueuePair>(isClient, length);
        cout << length << ", WriteImmRC, ";
        runWriteWithImm<rdma::RcQueuePair>(isClient, length);
    }
}
