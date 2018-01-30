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

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    static constexpr std::string_view data = "123456789012345678901234567890123456789012345678901234567890123\0"sv;
    auto net = rdma::Network();
    auto cq = net.newCompletionQueuePair();
    auto qp = rdma::QueuePair(net, ibv::queuepair::Type::RC, cq);

    std::array<char, data.size()> recvbuf{};
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(), {ibv::AccessFlag::LOCAL_WRITE});
    std::array<char, data.size()> sendbuf{};
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto socket = tcp_socket();
    if (isClient) {
        {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &addr.sin_addr);
            tcp_connect(socket, addr);
        }

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        qp.connect(remoteAddr);


        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
        send.setLocalAddress(sendmr->getSlice());
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
                qp.postWorkRequest(send);

                if (cq.pollRecvCompletionQueueBlocking() != 42) {
                    throw;
                }

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
        qp.connect(remoteAddr);


        auto send = ibv::workrequest::Simple<ibv::workrequest::Send>{};
        send.setLocalAddress(sendmr->getSlice());
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
                std::copy(recvbuf.begin(), recvbuf.end(), sendbuf.begin());
                // echo back the received data
                qp.postWorkRequest(send);
            }
        }, 1);

        tcp_close(acced);
    }

    tcp_close(socket);
}