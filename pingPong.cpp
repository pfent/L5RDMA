#include <iostream>
#include <algorithm>
#include <chrono>
#include <array>
#include <memory>
#include <exchangableTransports/transports/Transport.h>
#include <exchangableTransports/transports/TcpTransport.h>
#include <exchangableTransports/transports/DomainSocketsTransport.h>

using namespace std;
using namespace std::string_view_literals;

static const size_t MESSAGES = 1024 * 128;

template<class T>
class Ping {
    static constexpr string_view data = "123456789012345678901234567890123456789012345678901234567890123\0"sv;
    unique_ptr<TransportClient<T>> transport;
    array<uint8_t, data.size()> buffer;
public:
    explicit Ping(unique_ptr<TransportClient<T>> transport, string_view ip) : transport(move(transport)) {
        transport->connect(ip);
    }

    void ping() {
        transport->write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
        fill(begin(buffer), end(buffer), 0);
        transport->read(buffer.data(), buffer.size());
        for (size_t i = 0; i < data.size(); ++i) {
            if (buffer[i] != data[i]) {
                throw runtime_error{"received unexpected data: " + string(begin(buffer), end(buffer))};
            }
        }
    }
};

template<class T, size_t messageSize = 64>
class Pong {
    unique_ptr<TransportServer<T>> transport;
    array<uint8_t, messageSize> buffer;
public:
    explicit Pong(unique_ptr<TransportServer<T>> transport) : transport(move(transport)) {
    };

    void start() {
        transport->accept();
    }

    void pong() {
        transport->read(buffer.data(), buffer.size());
        transport->write(buffer.data(), buffer.size());
    }
};

int main(int argc, char **argv) {
    if (argc < 3 || (argv[1][0] == 'c' && argc < 4)) {
        cout << "Usage: " << argv[0] << " <client / server> <Port> [IP (if client)]" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = argv[2];

    if (isClient) {
        const auto ip = argv[3];
        auto client = Ping(make_transportClient<DomainSocketsTransportClient>(), port);
        const auto start = chrono::steady_clock::now();
        for (size_t i = 0; i < MESSAGES; ++i) {
            client.ping();
        }
        const auto end = chrono::steady_clock::now();
        const auto msTaken = chrono::duration<double, milli>(end - start).count();
        const auto sTaken = msTaken / 1000;
        cout << MESSAGES << " messages exchanged in " << msTaken << "ms" << endl;
        cout << MESSAGES / sTaken << " msg/s" << endl;
    } else {
        auto server = Pong(make_transportServer<DomainSocketsTransportServer>(port));
        server.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            server.pong();
        }
    }

    return 0;
}

