#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "rdma/CompletionQueuePair.hpp"
#include "tcpWrapper.h"
#include "RDMAMessageBuffer.h"

using namespace std;
using namespace rdma;

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = ::atoi(argv[2]);

    static const size_t MESSAGES = 1024 * 128;
    static const size_t BUFFERSIZE = 1024 * 16; // 16K
    static const size_t minSize = 1;
    static const size_t maxSize = 512;

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        auto sock = tcp_socket();
        tcp_connect(sock, addr);

        RDMAMessageBuffer rdma(BUFFERSIZE, sock);

        cout << "msgSize,inline,noninline" << endl;

        for (size_t msgSize = minSize; msgSize <= maxSize; ++msgSize) {

            auto sendData = vector<uint8_t>(msgSize);
            for (size_t i = 0; i < msgSize; ++i) {
                uint8_t random = rand() % 64;
                sendData[i] = '0' + random;
            }

            cout << msgSize << ',';

            {
                const auto start = chrono::steady_clock::now();
                for (size_t i = 0; i < MESSAGES; ++i) {
                    rdma.send(sendData.data(), sendData.size(), true);
                    auto answer = rdma.receive();
                    if (answer.size() != sendData.size()) {
                        throw runtime_error{"answer has wrong size!"};
                    }
                    for (size_t j = 0; j < sendData.size(); ++j) {
                        if (answer[j] != sendData[j]) {
                            throw runtime_error{"expected " + string(sendData.begin(), sendData.end()) + ", received " +
                                                string(answer.begin(), answer.end())};
                        }
                    }
                }
                const auto end = chrono::steady_clock::now();
                const auto msTaken = chrono::duration<double, milli>(end - start).count();
                const auto sTaken = msTaken / 1000;
                cout << MESSAGES / sTaken << ',';
            }

            {
                const auto start = chrono::steady_clock::now();
                for (size_t i = 0; i < MESSAGES; ++i) {
                    rdma.send(sendData.data(), sendData.size(), false);
                    auto answer = rdma.receive();
                    if (answer.size() != sendData.size()) {
                        throw runtime_error{"answer has wrong size!"};
                    }
                    for (size_t j = 0; j < sendData.size(); ++j) {
                        if (answer[j] != sendData[j]) {
                            throw runtime_error{"expected " + string(sendData.begin(), sendData.end()) + ", received " +
                                                string(answer.begin(), answer.end())};
                        }
                    }
                }
                const auto end = chrono::steady_clock::now();
                const auto msTaken = chrono::duration<double, milli>(end - start).count();
                const auto sTaken = msTaken / 1000;
                cout << MESSAGES / sTaken << endl;
            }
        }
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        auto sock = tcp_socket();
        tcp_bind(sock, addr);
        listen(sock, SOMAXCONN);
        sockaddr_in inAddr;

        auto acced = tcp_accept(sock, inAddr);

        RDMAMessageBuffer rdma(BUFFERSIZE, acced);

        for (size_t msgSize = minSize; msgSize <= maxSize; ++msgSize) {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto ping = rdma.receive();
                rdma.send(ping.data(), ping.size());
            }
        }

        for (size_t msgSize = minSize; msgSize <= maxSize; ++msgSize) {
            for (size_t i = 0; i < MESSAGES; ++i) {
                auto ping = rdma.receive();
                rdma.send(ping.data(), ping.size());
            }
        }

        close(acced);
        close(sock);
    }
    return 0;
}

