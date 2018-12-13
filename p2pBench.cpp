#include <iostream>
#include <thread>
#include <util/ycsb.h>
#include "rdma/Network.hpp"
#include "util/bench.h"
#include <unistd.h>
#include <include/RdmaTransport.h>

using namespace std;
using namespace l5::transport;

static constexpr uint16_t port = 1234;
static const char *ip = "127.0.0.1";
static constexpr auto MESSAGES = 1024 * 1024;

template<class Client, class Server>
void doRun(bool isClient, size_t size) {
    if (isClient) {
        RandomString rand;
        std::vector<uint8_t> testdata(size);
        rand.fill(size, reinterpret_cast<char *>(testdata.data()));

        sleep(1);
        auto client = Client();
        for (int i = 0;; ++i) {
            try {
                client.connect(string(ip) + ":" + to_string(port));
                break;
            } catch (...) {
                std::this_thread::sleep_for(20ms);
                if (i > 10) throw;
            }
        }

        std::vector<uint8_t> buf(size);

        for (size_t m = 0; m < MESSAGES; ++m) {
            client.write(testdata.data(), size);
            client.read(buf.data(), size);

            for (size_t i = 0; i < size; ++i) {
                if (testdata[i] != buf[i]) throw runtime_error("NEQ");
            }
        }
    } else {
        auto server = Server(to_string(port));
        server.accept();

        std::vector<uint8_t> buf(size);
        bench(MESSAGES, [&] {
            for (size_t m = 0; m < MESSAGES; ++m) {
                server.read(buf.data(), size);
                server.write(buf.data(), size);
            }
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <client / server> messagesize <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto size = atoi(argv[2]);
    if (argc > 3) {
        ip = argv[3];
    }

    cout << "size, messages, seconds, msgps, user, kernel, total\n";
    if (!isClient) {
        cout << size << ", ";
    }
    doRun<RdmaTransportClient<>, RdmaTransportServer<>>(isClient, size);
}
