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

int tcp_socket() {
    auto sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw runtime_error{"Could not open socket"};
    }
    return sock;
}

void tcp_connect(int sock, sockaddr_in &addr) {
    if (connect(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw runtime_error{"error connect'ing"};
    }
}

void tcp_write(int sock, void *buffer, size_t size) {
    if (write(sock, buffer, size) < 0) {
        throw runtime_error{"error write'ing"};
    }
}

void tcp_read(int sock, void *buffer, size_t size) {
    if (read(sock, buffer, size) < 0) {
        throw runtime_error{"error read'ing"};
    }
}

void tcp_bind(int sock, sockaddr_in &addr) {
    if (bind(sock, (sockaddr *) &addr, sizeof addr) < 0) {
        throw runtime_error{"error bind'ing"};
    }
}

struct RDMA_Info {
    uint32_t QPN; // TODO qpn probably should be separate, because we can't setup a RMR without a network
    uint32_t rmrKey;
    uintptr_t rmrAddress;
};

int tcp_accept(int sock, sockaddr_in &inAddr) {
    socklen_t inAddrLen = sizeof inAddr;
    auto acced = accept(sock, (sockaddr *) &inAddr, &inAddrLen);
    if (acced < 0) {
        throw runtime_error{"error accept'ing"};
    }
    return acced;
}

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = atoi(argv[2]);

    auto sock = tcp_socket();

    static const size_t BUFFER_SIZE = 64;
    uint8_t buffer[BUFFER_SIZE];
    const char DATA[] = "123456789012345678901234567890123456789012345678901234567890123";
    static_assert(BUFFER_SIZE == sizeof(DATA), "DATA needs the right size ");

    // TODO:
    // Setup Network && CompletionQueuePair && QueuePair

    if (isClient) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, argv[3], &addr.sin_addr);

        // TODO:
        // Recieve the qpn
        // Setup the Network
        // Setup shared memory region
        // Exchange RMR keys && addresses
        // Setup the Remote memory region

        tcp_connect(sock, addr);
        copy(begin(DATA), end(DATA), begin(buffer));
        const auto start = chrono::steady_clock::now();
        tcp_write(sock, buffer, BUFFER_SIZE);
        fill(begin(buffer), end(buffer), 0);
        tcp_read(sock, buffer, BUFFER_SIZE);
        if (not equal(begin(buffer), end(buffer), begin(DATA))) {
            throw runtime_error{"expected '" + string(DATA) + "', received " + string(begin(buffer), end(buffer))};
        }
        const auto end = chrono::steady_clock::now();
        cout << "RTT: " << chrono::duration<double, milli>(end - start).count() << "ms" << endl;
    } else {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        // TODO:
        // Recieve the qpn
        // Setup the Network
        // Setup shared memory region
        // Exchange RMR keys && addresses
        // Setup the Remote memory region

        tcp_bind(sock, addr);
        listen(sock, SOMAXCONN);
        sockaddr_in inAddr;
        for (;;) {
            auto acced = tcp_accept(sock, inAddr);
            tcp_read(acced, buffer, BUFFER_SIZE);
            cout << "received: '";
            for (const auto c : buffer) {
                cout << c;
            }
            cout << "'" << endl;
            tcp_write(acced, buffer, BUFFER_SIZE);
            close(acced);
        }
    }

    close(sock);
    return 0;
}

