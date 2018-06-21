#include <iostream>
#include <include/MulticlientRDMATransport.h>
#include "include/Transport.h"
#include "include/TcpTransport.h"
#include "include/DomainSocketsTransport.h"
#include "include/SharedMemoryTransport.h"
#include "apps/PingPong.h"
#include "util/bench.h"
#include "include/RdmaTransport.h"
#include "include/LibRdmacmTransport.h"

using namespace std;
using namespace l5::transport;

static const size_t MESSAGES = 1024 * 1024;
static const char *ip = "127.0.0.1";
static constexpr uint16_t port = 1234;

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    cout << "size, connection, messages, time, msgps, user, system, total\n";
    for (const size_t size : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        if (isClient) {
            std::vector<uint8_t> testdata(size);
            for (size_t i = 0; i < size; ++i) {
                testdata[i] = i % 256;
            }
            sleep(1);
            {
                cout << size << ", " << "normal, ";
                auto client = RdmaTransportClient();
                client.connect(ip + string(":") + to_string(port));
                std::vector<uint8_t> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.write(testdata.data(), size);
                        client.read(buf.data(), size);

                        for (size_t j = 0; j < size; ++j) {
                            if (testdata[j] != buf[j]) throw runtime_error("NEQ");
                        }
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "zerocopy, ";
                auto client = RdmaTransportClient();
                client.connect(ip + string(":") + to_string(port));
                std::vector<char> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.writeZC([&](auto begin) {
                            std::copy(testdata.data(), testdata.data() + size, begin);
                            return size;
                        });
                        client.readZC([&](auto begin, auto end) {
                            if (not std::equal(begin, end, testdata.data())) throw runtime_error("NEQ");
                        });
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "many, ";
                auto client = MultiClientRDMATransportClient();
                client.connect(ip, port);
                std::vector<uint8_t> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.send(testdata.data(), size);
                        client.receive(buf.data(), size);

                        for (size_t j = 0; j < size; ++j) {
                            if (testdata[j] != buf[j]) throw runtime_error("NEQ");
                        }
                    }
                });
            }
            sleep(1);
            {
                cout << size << ", " << "many_zerocopy, ";
                auto client = MultiClientRDMATransportClient();
                client.connect(ip, port);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        client.send([&](auto begin) {
                            std::copy(testdata.data(), testdata.data() + size, begin);
                            return size;
                        });
                        client.receive([&](auto begin, auto end) {
                            if (not std::equal(begin, end, testdata.data())) throw runtime_error("NEQ");
                        });
                    }
                });
            }
        } else {
            {
                cout << size << ", " << "normal, ";
                auto server = RdmaTransportServer(to_string(port));
                server.accept();
                std::vector<uint8_t> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        server.read(buf.data(), size);
                        server.write(buf.data(), size);
                    }
                });
            }
            {
                cout << size << ", " << "zerocopy, ";
                auto server = RdmaTransportServer(to_string(port));
                server.accept();
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        server.readZC([&](auto begin, auto end) -> void {
                            server.writeZC([&](auto writeBegin) -> size_t {
                                std::copy(begin, end, writeBegin);
                                return std::distance(begin, end);
                            });
                        });
                    }
                });
            }
            {
                cout << size << ", " << "many, ";
                auto server = MulticlientRDMATransportServer(to_string(port));
                server.accept();
                std::vector<uint8_t> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        const auto client = server.receive(buf.data(), size);
                        server.send(client, buf.data(), size);
                    }
                });
            }
            {
                cout << size << ", " << "many, ";
                auto server = MulticlientRDMATransportServer(to_string(port));
                server.accept();
                std::vector<uint8_t> buf(size);
                bench(MESSAGES, [&]() {
                    for (size_t i = 0; i < MESSAGES; ++i) {
                        server.receive([&](auto sender, auto begin, auto end) -> void {
                            server.send(sender, [&](auto writeBegin) -> size_t {
                                std::copy(begin, end, writeBegin);
                                return std::distance(begin, end);
                            });
                        });
                    }
                });
            }
        }
    }

    return 0;
}

