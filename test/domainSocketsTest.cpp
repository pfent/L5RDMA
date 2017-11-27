#include <iostream>
#include <future>
#include <exchangeableTransports/transports/DomainSocketsTransport.h>
#include <exchangeableTransports/apps/PingPong.h>

using namespace std;

const size_t DOMAIN_MESSAGES = 256 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    auto pong = Pong(make_transportServer<DomainSocketsTransportServer>("/tmp/pingPong"));
    const auto server = std::async(std::launch::async, [&]() {
        pong.start();
        for (size_t i = 0; i < DOMAIN_MESSAGES; ++i) {
            pong.pong();
        }
        return DOMAIN_MESSAGES;
    });

    auto ping = Ping(make_transportClient<DomainSocketsTransportClient>(), "/tmp/pingPong");
    const auto client = std::async(std::launch::async, [&]() {
        for (size_t i = 0; i < DOMAIN_MESSAGES; ++i) {
            ping.ping();
        }
        return DOMAIN_MESSAGES;
    });

    const auto serverStatus = server.wait_for(std::chrono::seconds(TIMEOUT_IN_SECONDS));
    const auto clientStatus = client.wait_for(std::chrono::seconds(TIMEOUT_IN_SECONDS));

    if (serverStatus != std::future_status::ready || clientStatus != std::future_status::ready) {
        std::cerr << "timeout" << std::endl;
        return -1;
    }

    return 0;
}

