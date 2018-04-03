#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <transports/MulticlientTransport.h>
#include "libibverbscpp/libibverbscpp.h"
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "util/tcpWrapper.h"
#include "util/bench.h"
#include "rdma/RcQueuePair.h"
#include "rdma/UcQueuePair.h"
#include "rdma/UdQueuePair.h"

using namespace std;

static constexpr uint16_t port = 1234;
static const char *ip = "127.0.0.1";
static constexpr auto MESSAGES = 1024 * 1024;

void doRun(size_t clients, bool isClient) {
    if (isClient) {
        const char *testdata = "asdfghjkl";

        std::vector<std::thread> clientThreads;
        for (size_t c = 0; c < clients; ++c) {
            clientThreads.emplace_back([&] {
                auto client = MultiClientTransportClient();
                for (int i = 0;; ++i) {
                    try {
                        client.connect(ip, port);
                        break;
                    } catch (...) {
                        std::this_thread::sleep_for(20ms);
                        if (i > 10) throw;
                    }
                }

                std::vector<char> buf(10);

                for (size_t m = 0; m < MESSAGES; ++m) {
                    client.send(reinterpret_cast<const uint8_t *>(testdata), 10);
                    client.receive(buf.data(), 10);

                    for (size_t i = 0; i < 10; ++i) {
                        if (testdata[i] != buf[i]) throw runtime_error("NEQ");
                    }
                }
            });
        }
        for (auto &t : clientThreads) {
            t.join();
        }
    } else {
        auto server = MulticlientTransportServer(to_string(port));
        for (size_t i = 0; i < clients; ++i) {
            server.accept();
        }

        std::vector<uint8_t> buf(10);
        bench(MESSAGES * clients, [&] {
            for (size_t m = 0; m < MESSAGES * clients; ++m) {
                auto client = server.receive(buf.data(), 10);
                server.send(client, buf.data(), 10);
            }
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    cout << "clients, messages, seconds, msgps, user, kernel, total\n";
    for (auto clients : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}) {
        if (!isClient) {
            cout << clients << ", ";
        }
        doRun(clients, isClient);
    }
}
