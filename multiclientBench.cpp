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

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    if (isClient) {
        auto client = MultiClientTransportClient();
        client.connect(ip, port);

        const char *testdata = "asdfghjkl";
        std::vector<char> buf(10);

        bench(MESSAGES, [&]() {
            for (size_t m = 0; m < MESSAGES; ++m) {
                client.send(reinterpret_cast<const uint8_t *>(testdata), 10);
                client.receive(buf.data(), 10);

                for (size_t i = 0; i < 10; ++i) {
                    if (testdata[i] != buf[i]) throw runtime_error("NEQ");
                }
            }
        });
    } else {
        auto server = MulticlientTransportServer(to_string(port));
        server.accept();

        std::vector<uint8_t> buf(10);
        bench(MESSAGES, [&]() {
            for (size_t m = 0; m < MESSAGES; ++m) {
                auto client = server.receive(buf.data(), 10);
                server.send(client, buf.data(), 10);
            }
        });
    }
}
