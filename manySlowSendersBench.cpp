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
        std::vector<std::vector<double>> latencySamples(threads);
        std::vector<std::thread> clientThreads;

        for (size_t c = 0; c < threads; ++c) {
            clientThreads.emplace_back([&, c] {
                auto rand = Random32();
                const auto lookupKeys = generateZipfLookupKeys(msgps * duration);
                auto client = MultiClientTransportClient();
                tryConnect(client);

                auto response = ReadResponse{};
                static constexpr size_t timedMessages = 50;

                for (size_t i = 0; i < lookupKeys.size(); i += timedMessages) {
                    using namespace std::chrono;
                    const auto start = high_resolution_clock::now();

                    for (size_t j = 0; j < timedMessages; ++j) {
                        const auto field = rand.next() % ycsb_field_count;
                        const auto message = ReadMessage{lookupKeys[i], field};
                        client.write(message);
                        client.read(response);
                        benchmark::DoNotOptimize(response);
                        ++counters[c];
                    }

                    const auto end = std::chrono::high_resolution_clock::now();
                    const auto muSecs = chrono::duration<double, micro>(end - start).count();
                    latencySamples[c].push_back(muSecs / timedMessages);

                    this_thread::sleep_for(
                            duration_cast<nanoseconds>(chrono::duration<double>(double(timedMessages) / msgps)));
                }
            });
        }
        for (auto &t : clientThreads) {
            t.join();
        }

        std::vector<double> roundedSamples;
        for (const auto &samples : latencySamples) {
            for (const auto sample : samples) {
                roundedSamples.push_back(std::nearbyint(sample * 100) / 100); // round to 2 decimals
            }
        }

        std::map<double, size_t> summed;
        for (const auto sample : roundedSamples) {
            ++summed[sample];
        }

        std::cout << "msgps, latency, count\n";
        for (const auto[latency, count] : summed) {
            cout << msgps * threads << ", " << latency << ", " << count << '\n';
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
        }
    }
}

int main(int argc, char **argv) {
    // TODO: this sporadically doesn't finish with one thread being stuck polling the very first work completion
    // FIXME: maybe only *drain* the completionqueue instead of waiting for completion

    /* It's dangerous to go alone!
     * Take this.
     *
     * rm out; while [[ ! -s out ]]; do NODE=1; timeout 15 numactl --membind=$NODE --cpunodebind=$NODE ./manySlowSendersBench server; done;
     * while [[ ! -s out ]]; do NODE=1; timeout 15 numactl --membind=$NODE --cpunodebind=$NODE ./manySlowSendersBench client | tee out; done; cp out sample.csv;
     */
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    for (const size_t msgps : {30000u, 40000u, 50000u, 60000u}) {
        doRun(msgps, isClient);
    }
}
