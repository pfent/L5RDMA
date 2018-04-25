#include <iostream>
#include <thread>
#include <iomanip>
#include "transports/MulticlientTransport.h"
#include "util/bench.h"
#include "util/ycsb.h"
#include "util/Random32.h"

using namespace std;

static constexpr size_t threadsPerClient = 10;
static size_t numberOfClients = 1;
static constexpr uint16_t port = 1234;
static constexpr size_t duration = 10; // seconds
static constexpr size_t warmupCount = 1000;
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
        std::vector<size_t> counters(threadsPerClient);
        std::vector<std::vector<double>> latencySamples(threadsPerClient);
        std::vector<std::thread> clientThreads;

        for (size_t c = 0; c < threadsPerClient; ++c) {
            clientThreads.emplace_back([&, c] {
                auto rand = Random32();
                const auto lookupKeys = generateZipfLookupKeys(msgps * duration);
                auto client = MultiClientTransportClient();
                tryConnect(client);

                auto response = ReadResponse{};
                for (size_t i = 0; i < warmupCount; ++i) {
                    const auto field = rand.next() % ycsb_field_count;
                    const auto message = ReadMessage{lookupKeys[i], field};
                    client.write(message);
                    client.read(response);
                    benchmark::DoNotOptimize(response);
                }


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
            cout << msgps * threadsPerClient << ", " << latency << ", " << count << '\n';
        }
    } else { // server
        const auto database = YcsbDatabase();
        auto server = MulticlientTransportServer(to_string(port));
        std::cout << "Letting " << numberOfClients << " clients connect\n";
        for (size_t i = 0; i < numberOfClients * threadsPerClient; ++i) {
            if (i % threadsPerClient == 0) std::cout << "Waiting for client " << i / threadsPerClient << '\n';
            server.accept();
        }

        std::cout << "Warming up\n";
        auto message = ReadMessage{};
        // warmup
        for (size_t i = 0; i < numberOfClients * threadsPerClient * warmupCount; ++i) {
            auto client = server.read(message);
            auto&[lookupKey, field] = message;
            server.send(client, [&](auto begin) {
                database.lookup(lookupKey, field, begin);
                return ycsb_field_length;
            });
        }

        std::cout << "Benchmarking...\n";
        for (size_t m = 0; m < msgps * duration * numberOfClients * threadsPerClient; ++m) {
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
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <client / server> #messages [#clients] [127.0.0.1]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto msgps = atoi(argv[2]);
    if (argc > 3) {
        numberOfClients = atoi(argv[3]);
    }
    if (argc > 4) {
        ip = argv[4];
    }
    doRun(msgps, isClient);
}
