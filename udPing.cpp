#include <iostream>
#include <exchangeableTransports/util/tcpWrapper.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libibverbscpp/libibverbscpp.h>
#include <exchangeableTransports/rdma/Network.hpp>
#include <exchangeableTransports/rdma/QueuePair.hpp>
#include <exchangeableTransports/util/bench.h>

using namespace std;

constexpr uint16_t port = 1234;
constexpr auto ip = "127.0.0.1";
const size_t SHAREDMEM_MESSAGES = 1024 * 8;

const auto selectiveSignaledPostWr = [](size_t i, auto &send, auto &qp, auto &cq) {
    if (i % (8 * 1024) == 0) {
        send.setFlags({ibv::workrequest::Flags::INLINE, ibv::workrequest::Flags::SIGNALED});
        qp.postWorkRequest(send);
        send.setFlags({ibv::workrequest::Flags::INLINE});
        //cq.waitForCompletionSend();
    } else {
        qp.postWorkRequest(send);
    }
};

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    auto net = rdma::Network();
    auto cq = net.newCompletionQueuePair(); // TODO: use the scq from network
    auto qp = rdma::QueuePair(net, ibv::queuepair::Type::UD, cq);

    std::unique_ptr<ibv::ah::AddressHandle> ah;
    rdma::Address remoteAddr{};

    auto socket = tcp_socket();
    int acced;
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            tcp_connect(socket, addr);
        }

        remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        {
            ibv::ah::Attributes ahAttributes{};
            ahAttributes.setIsGlobal(false);
            ahAttributes.setDlid(remoteAddr.lid);
            ahAttributes.setSl(0);
            ahAttributes.setSrcPathBits(0);
            ahAttributes.setPortNum(1); // local port

            ah = net.getProtectionDomain().createAddressHandle(ahAttributes);
        }
    } else {
        {   // setup tcp socket
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            tcp_bind(socket, addr);
            tcp_listen(socket);
        }

        acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);

        {
            ibv::ah::Attributes ahAttributes{};
            ahAttributes.setIsGlobal(false);
            ahAttributes.setDlid(remoteAddr.lid);
            ahAttributes.setSl(0);
            ahAttributes.setSrcPathBits(0);
            ahAttributes.setPortNum(1); // local port

            ah = net.getProtectionDomain().createAddressHandle(ahAttributes);
        }
    }

    for (const size_t length : {1, 2, 4, 8, 16, 32, 64, 128, 256}) {
        std::string data = "\0";
        while (data.size() < length) {
            data = "0" + data;
        }

        // from `man ibv_post_recv`:
        // [for UD:] in all cases, the actual data of the incoming message will start at an offset of 40 bytes into the buffer
        std::vector<char> recvbuf(40 + data.size());
        auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(), {ibv::AccessFlag::LOCAL_WRITE});
        std::vector<char> sendbuf(data.size());
        auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

        if (isClient) {
            std::copy(data.begin(), data.end(), sendbuf.begin());

            auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
            send.setLocalAddress(sendmr->getSlice());
            send.setUDAddressHandle(*ah);
            send.setUDRemoteQueue(remoteAddr.qpn, 0x22222222);
            send.setInline();

            auto recv = ibv::workrequest::Recv{};
            recv.setId(42);
            auto receiveInfo = recvmr->getSlice();
            recv.setSge(&receiveInfo, 1);

            bench(SHAREDMEM_MESSAGES, [&]() {
                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                    std::fill(recvbuf.begin(), recvbuf.end(), 0);

                    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
                    qp.postRecvRequest(recv);
                    selectiveSignaledPostWr(i, send, qp, cq);

                    if (cq.pollRecvCompletionQueueBlocking() != 42) {
                        throw;
                    }

                    // check if the data is still the same
                    if (not std::equal(recvbuf.begin() + 40, recvbuf.end(), data.begin(), data.end())) {
                        throw;
                    }
                }
            }, 1);

        } else {
            auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
            send.setLocalAddress(sendmr->getSlice());
            send.setUDAddressHandle(*ah);
            send.setUDRemoteQueue(remoteAddr.qpn, 0x22222222);
            send.setInline();

            auto recv = ibv::workrequest::Recv{};
            recv.setId(42);
            auto receiveInfo = recvmr->getSlice();
            recv.setSge(&receiveInfo, 1);

            bench(SHAREDMEM_MESSAGES, [&]() {
                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                    // receive into buf
                    qp.postRecvRequest(recv);
                    if (cq.pollRecvCompletionQueueBlocking() != 42) {
                        throw;
                    }
                    std::copy(recvbuf.begin() + 40, recvbuf.end(), sendbuf.begin());
                    // echo back the received data
                    selectiveSignaledPostWr(i, send, qp, cq);
                }
            }, 1);
        }
    }

    if (not isClient) {
        tcp_close(acced);
    }

    tcp_close(socket);
}