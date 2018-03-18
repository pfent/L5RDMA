#include <iostream>
#include <tbb/tbb.h>
#include "transports/Transport.h"
#include "apps/PingPong.h"
#include "util/bench.h"
#include "util/pinthread.h"
#include "transports/RdmaTransport.h"

using namespace std;

const size_t SHAREDMEM_MESSAGES = 4 * 1024 * 128 * 16;

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <client / server> <#threads>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto threads = atoi(argv[2]);

    tbb::task_scheduler_init taskScheduler(threads);
    const auto messagesPerThread = SHAREDMEM_MESSAGES / threads;

    if (isClient) {
        vector<Ping<RdmaTransportClient>> clients;
        for (int i = 0; i < threads; ++i) {
            const auto port = 1234 + i;
            const auto connectionString = string("127.0.0.1:") + to_string(port);
            clients.emplace_back(make_transportClient<RdmaTransportClient>(), connectionString);
        }

        cout << "implementation, messages, time, msg/s, user, system, total\n";
        {
            cout << "rdma, ";
            tbb::task_group g;
            bench(messagesPerThread * threads, [&]() {
                for (int i = 0; i < threads; ++i) {
                    g.run([&, i] {
                        for (size_t j = 0; j < messagesPerThread; ++j) {
                            clients[i].ping();
                        }
                    });
                }
                g.wait();
            }, 1);
        }
    } else {
        vector<Pong<RdmaTransportServer>> servers;
        for (int i = 0; i < threads; ++i) {
            const auto port = 1234 + i;
            servers.emplace_back(make_transportServer<RdmaTransportServer>(to_string(port)));
            servers.back().start();
        }

        cout << "implementation, messages, time, msg/s, user, system, total\n";
        {
            cout << "rdma, ";
            tbb::task_group g;
            bench(messagesPerThread * threads, [&]() {
                for (int i = 0; i < threads; ++i) {
                    g.run([&, i] {
                        for (size_t j = 0; j < messagesPerThread; ++j) {
                            servers[i].pong();
                        }
                    });
                }
                g.wait();
            }, 1);
        }
    }

    return 0;
}

