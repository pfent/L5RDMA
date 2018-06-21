#include <iostream>
#include <tbb/tbb.h>
#include <thread>
#include "transports/Transport.h"
#include "apps/PingPong.h"
#include "util/bench.h"
#include "transports/RdmaTransport.h"

using namespace std;
using namespace l5::transport;

static constexpr uint16_t port = 1234;
static const char *ip = "127.0.0.1";
static constexpr auto MESSAGES = 1024 * 1024;

void doRun(size_t clients, bool isClient) {
    if (isClient) {
        sleep(2);
        vector<Ping<RdmaTransportClient>> rdmaClients;
        for (size_t i = 0; i < clients; ++i) {
            rdmaClients.emplace_back(make_transportClient<RdmaTransportClient>(),
                                     ip + string(":") + to_string(port + i));
        }

        std::vector<std::thread> clientThreads;
        for (size_t i = 0; i < clients; ++i) {
            clientThreads.emplace_back([&, i] {
                for (size_t j = 0; j < MESSAGES; ++j) {
                    rdmaClients[i].ping();
                }
            });
        }
        for (auto &t : clientThreads) t.join();
    } else {
        vector<Pong<RdmaTransportServer>> servers;
        for (size_t i = 0; i < clients; ++i) {
            servers.emplace_back(make_transportServer<RdmaTransportServer>(to_string(port + i)));
            servers.back().start();
        }

        std::vector<std::thread> serverThreads;
        bench(MESSAGES * clients, [&] {
            for (size_t i = 0; i < clients; ++i) {
                serverThreads.emplace_back([&, i] {
                    for (size_t j = 0; j < MESSAGES; ++j) {
                        servers[i].pong();
                    }
                });
            }

            for (auto &t: serverThreads) t.join();
        }, 1);
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

    if (!isClient) {
        cout << "clients, messages, seconds, msgps, user, kernel, total\n";
    }
    for (size_t clients = 1; clients <= 32; ++clients) {
        if (!isClient) {
            cout << clients << ", ";
        }
        doRun(clients, isClient);
    }
}
