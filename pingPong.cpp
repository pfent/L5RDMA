#include <iostream>
#include <chrono>
#include <exchangeableTransports/transports/Transport.h>
#include <exchangeableTransports/transports/TcpTransport.h>
#include <exchangeableTransports/transports/DomainSocketsTransport.h>
#include <exchangeableTransports/transports/SharedMemoryTransport.h>
#include <exchangeableTransports/apps/PingPong.h>
#include <exchangeableTransports/util/bench.h>

using namespace std;

static const size_t MESSAGES = 1024 * 128; // approx 1s

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    if (isClient) {
        {
            cout << "shared memory, ";
            auto client = Ping(make_transportClient<SharedMemoryTransportClient>(), "/tmp/pingPong");
            bench([&]() {
                const auto start = chrono::steady_clock::now();
                for (size_t i = 0; i < MESSAGES; ++i) {
                    client.ping();
                }
                const auto end = chrono::steady_clock::now();
                const auto sTaken = chrono::duration<double>(end - start).count();
                cout << MESSAGES / sTaken << " msg/s, ";
            });
        }
    } else {
        {
            cout << "shared memory, ";
            auto server = Pong(make_transportServer<SharedMemoryTransportServer>("/tmp/pingPong"));
            server.start();
            bench([&]() {
                for (size_t i = 0; i < MESSAGES; ++i) {
                    server.pong();
                }
            });
        }
    }

    return 0;
}

