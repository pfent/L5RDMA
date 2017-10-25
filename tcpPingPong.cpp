#include <iostream>
#include <arpa/inet.h>
#include <algorithm>
#include <chrono>
#include "rdma/Network.hpp"
#include "rdma_tests/util/tcpWrapper.h"

using namespace std;
using namespace rdma;

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = atoi(argv[2]);

    auto sock = tcp_socket();

    static const size_t MESSAGES = 1024 * 128;
    static const size_t BUFFER_SIZE = 64;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    static_assert(BUFFER_SIZE == sizeof(buffer), "DATA needs the right size ");

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        tcp_connect(sock, addr);
        const auto start = chrono::steady_clock::now();
        for (size_t i = 0; i < MESSAGES; ++i) {
            tcp_write(sock, DATA, BUFFER_SIZE);

            fill(begin(buffer), end(buffer), 0);

            tcp_read(sock, buffer, BUFFER_SIZE);

            for (size_t j = 0; j < BUFFER_SIZE; ++j) {
                if (buffer[j] != DATA[j]) {
                    throw runtime_error{"expected '1~9', received " + string(begin(buffer), end(buffer))};
                }
            }
        }
        const auto end = chrono::steady_clock::now();
        const auto msTaken = chrono::duration<double, milli>(end - start).count();
        const auto sTaken = msTaken / 1000;
        cout << MESSAGES << " messages exchanged in " << msTaken << "ms" << endl;
        cout << MESSAGES / sTaken << " msg/s" << endl;
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        tcp_bind(sock, addr);
        tcp_listen(sock);
        sockaddr_in inAddr;
        for (;;) {
            const auto acced = tcp_accept(sock, inAddr);
            for (size_t i = 0; i < MESSAGES; ++i) {
                tcp_read(acced, buffer, BUFFER_SIZE);
                tcp_write(acced, buffer, BUFFER_SIZE);
            }
            tcp_close(acced);
        }
    }

    tcp_close(sock);
    return 0;
}

