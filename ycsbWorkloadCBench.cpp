#include <include/DomainSocketsTransport.h>
#include <include/TcpTransport.h>
#include <include/SharedMemoryTransport.h>
#include "include/RdmaTransport.h"
#include <array>
#include <vector>
#include <thread>
#include "util/bench.h"
#include "util/ycsb.h"
#include "util/Random32.h"
#include "util/doNotOptimize.h"

using namespace l5::transport;

static constexpr uint16_t port = 1234;
static std::string_view ip = "127.0.0.1";

void doRunNoCommunication() {
    const auto database = YcsbDatabase();
    auto rand = Random32();
    const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count * 10);
    std::array<char, ycsb_field_length> data{};

    std::cout << "none, ";
    bench(ycsb_tx_count * 10, [&] {
        for (auto lookupKey : lookupKeys) {
            const auto field = rand.next() % ycsb_field_count;
            database.lookup(lookupKey, field, data.begin());
            DoNotOptimize(data);
        }
    });
}

template<class Server, class Client>
void doRun(bool isClient, std::string connection) {
    struct ReadMessage {
        YcsbKey lookupKey;
        size_t field;
    };

    struct ReadResponse {
        std::array<char, ycsb_field_length> data;
    };

    if (isClient) {
        auto client = Client();

        for (int i = 0;; ++i) {
            try {
                client.connect(connection);
                break;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (i > 1000) throw;
            }
        }

        auto rand = Random32();
        const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);
        auto response = ReadResponse{};

        for (const auto lookupKey: lookupKeys) {
            const auto field = rand.next() % ycsb_field_count;
            const auto message = ReadMessage{lookupKey, field};
            client.write(message);
            client.read(response);
            DoNotOptimize(response);
        }
    } else { // server
        auto server = Server(connection);
        const auto database = YcsbDatabase();
        server.accept();
        bench(ycsb_tx_count, [&] {
            for (size_t i = 0; i < ycsb_tx_count; ++i) {
                auto message = ReadMessage{};
                server.read(message);
                auto&[lookupKey, field] = message;
                auto response = ReadResponse{};
                database.lookup(lookupKey, field, reinterpret_cast<char*>(&response));
                server.write(response);
            }
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <client / server> <[DS|SHM|TCP|RDMA]> <(IP, optional) 127.0.0.1>" << std::endl;
        return -1;
    }
    const auto isClient = std::string_view(argv[1]) == "client";
    const auto transportProtocol = std::string_view(argv[2]);
    if (argc >= 3) ip = argv[2];
    std::string connectionString;
    if (isClient) {
        connectionString = std::string(ip) + ":" + std::to_string(port);
    } else {
        connectionString = std::to_string(port);
    }
    if (!isClient) std::cout << "connection, transactions, time, msgps, user, system, total\n";
    if (!isClient) doRunNoCommunication();

    if (transportProtocol == "DS") {
        if (!isClient) std::cout << "domainSocket, ";
        doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>(isClient, "/tmp/testSocket");
    } else if (transportProtocol == "SHM") {
        if (!isClient) std::cout << "shared memory, ";
        doRun<SharedMemoryTransportServer<>, SharedMemoryTransportClient<>>(isClient, "/tmp/testSocket");
    } else if (transportProtocol == "TCP") {
        if (!isClient) std::cout << "tcp, ";
        doRun<TcpTransportServer, TcpTransportClient>(isClient, connectionString);
    } else if (transportProtocol == "RDMA") {
        if (!isClient) std::cout << "rdma, ";
        doRun<RdmaTransportServer<>, RdmaTransportClient<>>(isClient, connectionString);
    }
}
