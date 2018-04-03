#include <iostream>
#include "transports/Transport.h"
#include "transports/TcpTransport.h"
#include "transports/DomainSocketsTransport.h"
#include "transports/SharedMemoryTransport.h"
#include "apps/PingPong.h"
#include "util/bench.h"
#include "util/pinthread.h"
#include "transports/RdmaTransport.h"
#include "transports/LibRdmacmTransport.h"

using namespace std;

static const size_t MESSAGES = 256 * 1024;  //~ 1s
static const size_t SHAREDMEM_MESSAGES = 1024 * 1024;
static const char *ip = "127.0.0.1";
static constexpr uint16_t port = 1234;

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    cout << "size, connection, messages, time, msgps, user, system, total\n";
    for (const size_t size : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        if (isClient) {
            sleep(1);
            {
                cout << size << ", " << "domainsockets, ";
                auto client = Ping(make_transportClient<DomainSocketsTransportClient>(), "/dev/shm/pingPong", size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.ping();
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "shared memory, ";
                auto client = Ping(make_transportClient<SharedMemoryTransportClient>(), "/dev/shm/pingPong", size);
                bench(SHAREDMEM_MESSAGES, [&]() {
                    for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                        client.ping();
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "tcp, ";
                auto client = Ping(make_transportClient<TcpTransportClient>(), ip + string(":") + to_string(port),
                                   size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.ping();
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "rdma, ";
                auto client = Ping(make_transportClient<RdmaTransportClient>(), ip + string(":") + to_string(port),
                                   size);
                bench(SHAREDMEM_MESSAGES, [&]() {
                    for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                        client.ping();
                    }
                });
            }
//        { // librdmacm doesn't seem to work with the current server config
//            cout << "librdmacm, ";
//            auto client = Ping(make_transportClient<LibRdmacmTransportClient>(), ip + string(":") + to_string(port));
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    client.ping();
//                }
//            }, 1);
//        }
        } else {

            {
                cout << size << ", " << "domainsockets, ";
                auto server = Pong(make_transportServer<DomainSocketsTransportServer>("/dev/shm/pingPong"), size);
                server.start();
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        server.pong();
                    }
                });
            }
            {
                cout << size << ", " << "shared memory, ";
                auto server = Pong(make_transportServer<SharedMemoryTransportServer>("/dev/shm/pingPong"), size);
                server.start();
                bench(SHAREDMEM_MESSAGES, [&]() {
                    for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                        server.pong();
                    }
                });
            }
            {
                cout << size << ", " << "tcp, ";
                auto server = Pong(make_transportServer<TcpTransportServer>(to_string(port)), size);
                server.start();
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        server.pong();
                    }
                });
            }
            {
                cout << size << ", " << "rdma, ";
                auto server = Pong(make_transportServer<RdmaTransportServer>(to_string(port)), size);
                server.start();
                bench(SHAREDMEM_MESSAGES, [&]() {
                    for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                        server.pong();
                    }
                });
            }
//        {
//            cout << "librdmacm, ";
//            auto server = Pong(make_transportServer<LibRdmacmTransportServer>(to_string(port)));
//            server.start();
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    server.pong();
//                }
//            }, 1);
//        }
        }
    }

    return 0;
}

