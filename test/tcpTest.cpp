#include "include/TcpTransport.h"
#include "apps/PingPong.h"
#include <future>
#include <iostream>

using namespace std;
using namespace l5::transport;

const size_t MESSAGES = 256 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    auto pong = Pong(make_transportServer<TcpTransportServer>("1234"));
    const auto server = std::async(std::launch::async, [&]() {
        pong.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            pong.pong();
        }
        return MESSAGES;
    });

    auto ping = Ping(make_transportClient<l5::transport::TcpTransportClient>(), "127.0.0.1:1234");
    const auto client = std::async(std::launch::async, [&]() {
        for (size_t i = 0; i < MESSAGES; ++i) {
            ping.ping();
        }
        return MESSAGES;
    });

    const auto serverStatus = server.wait_for(std::chrono::seconds(TIMEOUT_IN_SECONDS));
    const auto clientStatus = client.wait_for(std::chrono::seconds(TIMEOUT_IN_SECONDS));

    if (serverStatus != std::future_status::ready || clientStatus != std::future_status::ready) {
        std::cerr << "timeout" << std::endl;
        return -1;
    }
    return 0;
}

