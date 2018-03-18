#include <iostream>
#include "transports/Transport.h"
#include "transports/SharedMemoryTransport.h"
#include "util/bench.h"
#include "util/pinthread.h"
#include "transports/RdmaTransport.h"
#include "apps/KVStore.h"
#include <algorithm>
#include <random>

using namespace std;

const size_t MESSAGES = 256 * 1024; // ~ 1s
const size_t SHAREDMEM_MESSAGES = 4 * 1024 * 128 * 16;

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';

    if (isClient) {
        pinThread(0);
        std::vector<uint32_t> work(SHAREDMEM_MESSAGES);
        for (uint32_t i = 0; i < SHAREDMEM_MESSAGES; i++)
            work[i] = i;
        std::shuffle(work.begin(), work.end(), std::mt19937(std::random_device()()));

        cout << "implementation, messages, time, msg/s, user, system, total\n";
        {
            cout << "rdma, ";
            auto transport = make_transportClient<RdmaTransportClient>();
            transport->connect("127.0.0.1:1234");

            size_t i = 0;
            bench(SHAREDMEM_MESSAGES, [&]() {
                KvInput input{};
                if (i % 10 < 6) {
                    strcpy(input.command, "SELECT ");
                    input.key = work[i];
                    input.value = 0;
                    transport->write(reinterpret_cast<const uint8_t *>(&input), sizeof(input));

                    size_t response;
                    transport->read(reinterpret_cast<uint8_t *>(&response), sizeof(response));
                } else if (i % 10 < 8) {
                    strcpy(input.command, "INSERT ");
                    input.key = work[i];
                    input.value = work[i];
                    transport->write(reinterpret_cast<const uint8_t *>(&input), sizeof(input));
                } else {
                    strcpy(input.command, "DELETE ");
                    input.key = work[i];
                    input.value = 0;
                    transport->write(reinterpret_cast<const uint8_t *>(&input), sizeof(input));
                }
                ++i;
            }, 1);
        }
    } else {
        pinThread(1);
        cout << "implementation, messages, time, msg/s, user, system, total\n";
        {
            cout << "rdma, ";
            auto server = KVStore(make_transportServer<RdmaTransportServer>("1234"));
            server.start();
            bench(SHAREDMEM_MESSAGES, [&]() {
                server.respond();
            }, 1);
        }
    }

    return 0;
}



