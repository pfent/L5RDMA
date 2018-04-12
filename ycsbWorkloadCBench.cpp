#include <array>
#include <vector>
#include "transports/RdmaTransport.h"
#include "util/bench.h"
#include "util/ycsb.h"
#include "util/Random32.h"

static constexpr uint16_t port = 1234;
static const char *ip = "127.0.0.1";

void doRun(bool isClient) {
    struct ReadMessage {
        YcsbKey lookupKey;
        size_t field;
    };

    struct ReadResponse {
        std::array<char, ycsb_field_length> data;
    };

    if (isClient) {
        auto rand = Random32();
        const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);
        auto client = RdmaTransportClient();
        client.connect(ip + std::string(":") + std::to_string(port));
        auto response = ReadResponse{};

        for (const auto lookupKey: lookupKeys) {
            const auto field = rand.next() % ycsb_field_count;
            const auto message = ReadMessage{lookupKey, field};
            client.write(message);
            client.read(response);
            benchmark::DoNotOptimize(response);
        }
    } else { // server
        const auto database = YcsbDatabase();
        auto server = RdmaTransportServer(std::to_string(port));
        server.accept();
        std::cout << "transactions, time, msgps, user, system, total\n";
        bench(ycsb_tx_count, [&] {
            for (size_t i = 0; i < ycsb_tx_count; ++i) {
                auto message = ReadMessage{};
                server.read(message);
                auto&[lookupKey, field] = message;
                server.writeZC([&](auto begin) {
                    database.lookup(lookupKey, field, begin);
                    return ycsb_field_length;
                });
            }
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << std::endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }
    doRun(isClient);
}
