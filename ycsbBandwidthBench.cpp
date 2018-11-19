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
static const char *ip = "127.0.0.1";

void doRunNoCommunication() {
    const auto database = YcsbDatabase();
    const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count * 10);
    YcsbDataSet data{};

    std::cout << "none, ";
    bench(ycsb_tx_count * 10, [&] {
        for (auto lookupKey : lookupKeys) {
            database.lookup(lookupKey, data.begin());
            DoNotOptimize(data);
        }
    });
}

template<class Server, class Client>
void doRun(bool isClient, std::string connection) {
    struct ReadMessage {
        YcsbKey lookupKey;
    };

    struct ReadResponse {
        YcsbDataSet data;
    };

    if (isClient) {
        sleep(1);
        auto client = Client();

        for (int i = 0;; ++i) {
            try {
                client.connect(connection);
                break;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (i > 10) throw;
            }
        }

        std::cout << "connected to " << connection << '\n';

        const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);
        auto response = ReadResponse{};

        for (const auto lookupKey: lookupKeys) {
            const auto message = ReadMessage{lookupKey};
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
                server.write([&](auto begin) {
                    database.lookup(message.lookupKey, begin);
                    return sizeof(ReadResponse);
                });
            }
        });
    }
}

void doRunSharedMemory(bool isClient) {
    struct ReadMessage {
        YcsbKey lookupKey;
    };

    struct ReadResponse {
        YcsbDataSet data;
    };

    if (isClient) {
        sleep(1);
        auto client = SharedMemoryTransportClient();

        for (int i = 0;; ++i) {
            try {
                client.connect("/dev/shm/pingPong");
                break;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (i > 10) throw;
            }
        }

        std::cout << "connected to /dev/shm/pingPong\n";

        const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);

        for (const auto lookupKey : lookupKeys) {
            auto message = ReadMessage{lookupKey};
            auto response = ReadResponse();
            client.write(message);
            client.read(response); // TODO this is probably a bug in the Shared memory transport
            DoNotOptimize(response);
        }
    } else {
        auto server = SharedMemoryTransportServer("/dev/shm/pingPong");
        const auto database = YcsbDatabase();
        server.accept();
        bench(ycsb_tx_count, [&]() {
            for (size_t i = 0; i < ycsb_tx_count; ++i) {
                auto message = ReadMessage{};
                server.read(message);
                auto response = ReadResponse();
                database.lookup(message.lookupKey, response.data.begin());

                server.write(response);
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
    std::string connection;
    if (isClient) {
        connection = ip + std::string(":") + std::to_string(port);
    } else {
        connection = std::to_string(port);
    }
    if (argc > 2) {
        ip = argv[2];
    }
    std::cout << "connection, transactions, time, msgps, user, system, total\n";
    if (!isClient) doRunNoCommunication();
    std::cout << "domainSocket, ";
    doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>(isClient, "/tmp/testSocket");
    std::cout << "shared memory, ";
    doRunSharedMemory(isClient);
    std::cout << "tcp, ";
    doRun<TcpTransportServer, TcpTransportClient>(isClient, connection);
    std::cout << "rdma, ";
    doRun<RdmaTransportServer, RdmaTransportClient>(isClient, connection);
}