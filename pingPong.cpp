#include <iostream>
#include <algorithm>
#include <chrono>
#include <array>
#include <memory>
#include <exchangeableTransports/transports/Transport.h>
#include <exchangeableTransports/transports/TcpTransport.h>
#include <exchangeableTransports/transports/DomainSocketsTransport.h>
#include <exchangeableTransports/transports/SharedMemoryTransport.h>
#include <exchangeableTransports/apps/PingPong.h>
#include <fstream>
#include <vector>
#include <iterator>

using namespace std;
using namespace std::string_view_literals;

static const size_t MESSAGES = 1024 * 128; // approx 1s

double getGlobalStat() {
    auto globalStat = std::ifstream("/proc/stat", std::ios::in);
    if (not globalStat.is_open()) {
        throw std::runtime_error{"couldn't open /proc/stat"};
    }

    std::vector<std::string> ret;
    std::copy(std::istream_iterator<std::string>(globalStat),
              std::istream_iterator<std::string>(),
              std::back_inserter(ret));

    double user = stod(ret[1]), nice = stod(ret[2]), system = stod(ret[3]), idle = stod(ret[4]);
    return user + nice + system + idle;
}

std::tuple<double, double> getOwnStat() {
    auto ownStat = std::ifstream("/proc/self/stat");
    if (not ownStat.is_open()) {
        throw std::runtime_error{"couldn't open /proc/stat"};
    }

    std::vector<std::string> ret;
    std::copy(std::istream_iterator<std::string>(ownStat),
              std::istream_iterator<std::string>(),
              std::back_inserter(ret));

    double utime = stod(ret[13]), stime = stod(ret[14]);
    return {utime, stime};
}

template<typename T>
auto bench(T &&fun) {
    const auto before = getGlobalStat();
    const auto
    [ownUserBefore, ownSystemBefore] = getOwnStat();

    fun();

    const auto after = getGlobalStat();
    const auto
    [ownUserAfter, ownSystemAfter] = getOwnStat();

    const auto diff = (after - before);
    const auto userPercent = (ownUserAfter - ownUserBefore) / diff * 100.0;
    const auto systemPercent = (ownSystemAfter - ownSystemBefore) / diff * 100.0;
    const auto totalPercent = userPercent + systemPercent;

    cout << userPercent << ", " << systemPercent << ", " << totalPercent << '\n';
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <client / server> <connection (e.g. 127.0.0.1:1234)>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    const auto port = argv[2];

    if (isClient) {
        auto client = Ping(make_transportClient<TcpTransportClient>(), port);
        bench([&]() {
            const auto start = chrono::steady_clock::now();
            for (size_t i = 0; i < MESSAGES; ++i) {
                client.ping();
            }
            const auto end = chrono::steady_clock::now();
            const auto msTaken = chrono::duration<double, milli>(end - start).count();
            const auto sTaken = msTaken / 1000;
            cout << MESSAGES / sTaken << " msg/s" << endl;
        });
    } else {
        auto server = Pong(make_transportServer<TcpTransportServer>(port));
        server.start();
        bench([&]() {
            for (size_t i = 0; i < MESSAGES; ++i) {
                server.pong();
            }
        });
    }

    return 0;
}

