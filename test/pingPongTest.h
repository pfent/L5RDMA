#ifndef EXCHANGABLETRANSPORTS_PINGPONGTEST_H
#define EXCHANGABLETRANSPORTS_PINGPONGTEST_H

#include <iostream>
#include <future>
#include <exchangeableTransports/apps/PingPong.h>

template<typename TransportServer, typename TransportClient>
int pingPongTest(const size_t MESSAGES, const size_t TIMEOUT_IN_SECONDS) {
    auto pong = Pong(make_transportServer<TransportServer>("/tmp/pingPong"));
    const auto server = std::async(std::launch::async, [&]() {
        pong.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            pong.pong();
        }
        return MESSAGES;
    });

    auto ping = Ping(make_transportClient<TransportClient>(), "/tmp/pingPong");
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

#endif //EXCHANGABLETRANSPORTS_PINGPONGTEST_H
