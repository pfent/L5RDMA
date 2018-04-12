#include <iostream>
#include <thread>
#include <iomanip>
#include "transports/MulticlientTransport.h"
#include "util/bench.h"
#include "util/ycsb.h"
#include "util/Random32.h"

using namespace std;

static constexpr size_t threads = 100;
static constexpr uint16_t port = 1234;
static constexpr size_t duration = 10; // seconds
static const char *ip = "127.0.0.1";

void tryConnect(MultiClientTransportClient &client) {
    for (int i = 0;; ++i) {
        try {
            client.connect(ip, port);
            break;
        } catch (...) {
            std::this_thread::sleep_for(20ms);
            if (i > 100) throw;
        }
    }
}

void doRun(const size_t msgps, bool isClient) {
    struct ReadMessage {
        YcsbKey lookupKey;
        size_t field;
    };

    struct ReadResponse {
        std::array<char, ycsb_field_length> data;
    };

    if (isClient) {
        std::vector<size_t> counters(threads);
        std::vector<std::thread> clientThreads;

        for (size_t c = 0; c < threads; ++c) {
            clientThreads.emplace_back([&, c] {
                auto rand = Random32();
                const auto lookupKeys = generateZipfLookupKeys(msgps * duration);
                auto client = MultiClientTransportClient();
                tryConnect(client);

                auto response = ReadResponse{};

                for (const auto lookupKey: lookupKeys) {
                    const auto field = rand.next() % ycsb_field_count;
                    const auto message = ReadMessage{lookupKey, field};
                    client.write(message);
                    client.read(response);
                    benchmark::DoNotOptimize(response);
                    ++counters[c];

                    using namespace std::chrono;
                    this_thread::sleep_for(duration_cast<nanoseconds>(chrono::duration<double>(1e0 / msgps)));
                }
            });
        }
        for (auto &t : clientThreads) {
            t.join();
        }
    } else {
        const auto database = YcsbDatabase();
        auto server = MulticlientTransportServer(to_string(port));
        for (size_t i = 0; i < threads; ++i) {
            server.accept();
        }

        auto message = ReadMessage{};
        for (size_t m = 0; m < msgps * duration * threads; ++m) {
            auto client = server.read(message);
            auto&[lookupKey, field] = message;
            server.send(client, [&](auto begin) {
                database.lookup(lookupKey, field, begin);
                return ycsb_field_length;
            });
            if (m % (msgps * threads) == 0)
                std::cout << "completed:" << std::setw(3) << double(m * duration) / (msgps * threads) << "%\n";
        }
    }
}

int main(int argc, char **argv) {
    // TODO: this sporadically doesn't finish with one thread being stuck polling the very first work completion
    // FIXME: only *drain* the completionqueue instead of waiting for completion
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    //cout << "clients, messages, seconds, msgps, user, kernel, total\n";
    for (const size_t msgps : {1000u}) {
        doRun(msgps, isClient);
    }
}
