#include <iostream>
#include <exchangeableTransports/transports/Transport.h>
#include <exchangeableTransports/transports/TcpTransport.h>
#include <exchangeableTransports/transports/DomainSocketsTransport.h>
#include <exchangeableTransports/transports/SharedMemoryTransport.h>
#include <exchangeableTransports/apps/PingPong.h>
#include <exchangeableTransports/util/bench.h>
#include <exchangeableTransports/util/pinthread.h>
#include <exchangeableTransports/transports/RdmaTransport.h>
#include <exchangeableTransports/transports/LibRdmacmTransport.h>

using namespace std;

const size_t MESSAGES = 256 * 1024; // ~ 1s
const size_t SHAREDMEM_MESSAGES = 4 * 1024 * 128;

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    /**
     * TODO for benchmarking:
     * Compare IBV_SEND / IBV_WRITE
     * Additionally with IBV_SEND_INLINE
     * Compare with accelio
     */

    if (isClient) {
        pinThread(0);
        cout << "implementation, messages, time, msg/s, user, system, total\n";
//        {
//            cout << "domainsockets, ";
//            auto client = Ping(make_transportClient<DomainSocketsTransportClient>(), "/dev/shm/pingPong");
//            bench(MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    client.ping();
//                }
//            }, 5);
//        }
//        sleep(1);
//        {
//            cout << "shared memory, ";
//            auto client = Ping(make_transportClient<SharedMemoryTransportClient>(), "/dev/shm/pingPong");
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    client.ping();
//                }
//            }, 5);
//        }
//        sleep(1);
//        {
//            cout << "tcp, ";
//            auto client = Ping(make_transportClient<TcpTransportClient>(), "127.0.0.1:1234");
//            bench(MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    client.ping();
//                }
//            }, 5);
//        }
//        sleep(1);
//        {
//            cout << "rdma, ";
//            auto client = Ping(make_transportClient<RdmaTransportClient>(), "127.0.0.1:1234");
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    client.ping();
//                }
//            }, 1);
//        }
        {
            cout << "librdmacm, ";
            auto client = Ping(make_transportClient<LibRdmacmTransportClient>(), "127.0.0.1:1234");
            bench(SHAREDMEM_MESSAGES, [&]() {
                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                    client.ping();
                }
            }, 1);
        }
    } else {
        pinThread(1);
        cout << "implementation, messages, time, msg/s, user, system, total\n";
//        {
//            cout << "domainsockets, ";
//            auto server = Pong(make_transportServer<DomainSocketsTransportServer>("/dev/shm/pingPong"));
//            server.start();
//            bench(MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    server.pong();
//                }
//            }, 5);
//        }
//        {
//            cout << "shared memory, ";
//            auto server = Pong(make_transportServer<SharedMemoryTransportServer>("/dev/shm/pingPong"));
//            server.start();
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    server.pong();
//                }
//            }, 5);
//        }
//        {
//            cout << "tcp, ";
//            auto server = Pong(make_transportServer<TcpTransportServer>("1234"));
//            server.start();
//            bench(MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    server.pong();
//                }
//            }, 5);
//        }
//        {
//            cout << "rdma, ";
//            auto server = Pong(make_transportServer<RdmaTransportServer>("1234"));
//            server.start();
//            bench(SHAREDMEM_MESSAGES, [&]() {
//                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
//                    server.pong();
//                }
//            }, 1);
//        }
        {
            cout << "librdmacm, ";
            auto server = Pong(make_transportServer<LibRdmacmTransportServer>("1234"));
            server.start();
            bench(SHAREDMEM_MESSAGES, [&]() {
                for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                    server.pong();
                }
            }, 1);
        }
    }

    return 0;
}

