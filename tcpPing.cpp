#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include "rdma/Network.hpp"

using namespace std;
using namespace rdma;

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = atoi(argv[2]);

    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw runtime_error{"Could not open socket"};
    }

    static const size_t BUFFER_SIZE = 64;
    uint8_t buffer[BUFFER_SIZE];

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        if (connect(sock, (sockaddr *) &addr, sizeof addr) < 0) {
            throw runtime_error{"error connect'ing"};
        }
        unsigned char a = ':'; // ':' + 64 is 'z'
        for (auto &c : buffer) {
            c = a++;
        }
        const auto start = chrono::steady_clock::now();
        if (write(sock, buffer, BUFFER_SIZE) < 0) {
            throw runtime_error{"error write'ing"};
        }
        fill(begin(buffer), end(buffer), 0);
        if (read(sock, buffer, BUFFER_SIZE) < 0) {
            throw runtime_error{"error read'ing"};
        }
        a = ':';
        for (const auto c : buffer) {
            if (c != a++) {
                throw runtime_error{"expected ':~z', received " + string(begin(buffer), end(buffer))};
            }
        }
        const auto end = chrono::steady_clock::now();
        cout << "RTT: " << chrono::duration<double, milli>(end - start).count() << "ms" << endl;
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, (sockaddr *) &addr, sizeof addr) < 0) {
            throw runtime_error{"error bind'ing"};
        }
        listen(sock, SOMAXCONN);
        sockaddr_in inAddr;
        socklen_t inAddrLen = sizeof inAddr;
        for (;;) {
            auto acced = accept(sock, (sockaddr *) &inAddr, &inAddrLen);
            if (acced < 0) {
                throw runtime_error{"error accept'ing"};
            }
            if (read(acced, buffer, BUFFER_SIZE) < 0) {
                throw runtime_error{"error read'ing"};
            }
            cout << "received: '";
            for (const auto c : buffer) {
                cout << c;
            }
            cout << "'" << endl;
            if (write(acced, buffer, BUFFER_SIZE) < 0) {
                throw runtime_error{"error write'ing"};
            }
            close(acced);
        }
    }

    close(sock);
    return 0;
}

