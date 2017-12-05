#include <exchangeableTransports/transports/RdmaTransport.h>
#include <exchangeableTransports/apps/PingPong.h>
#include <future>
#include <iostream>
#include <sys/wait.h>
#include <zconf.h>

using namespace std;

const size_t MESSAGES = 4 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    const auto serverPid = fork();
    if (serverPid == 0) {
        auto pong = Pong(make_transportServer<RdmaTransportServer>("1234"));
        pong.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            pong.pong();
        }
        return 0;
    }

    const auto clientPid = fork();
    if (clientPid == 0) {
        sleep(1); // server needs some time to start
        auto ping = Ping(make_transportClient<RdmaTransportClient>(), "127.0.0.1:1234");
        for (size_t i = 0; i < MESSAGES; ++i) {
            ping.ping();
        }
    }

    size_t secs = 0;
    for (int status; secs < TIMEOUT_IN_SECONDS; ++secs, sleep(1)) {
        auto serverTerminated = waitpid(serverPid, &status, WNOHANG) != 0;
        auto clientTerminated = waitpid(clientPid, &status, WNOHANG) != 0;
        if (serverTerminated && clientTerminated) {
            break;
        }
    }

    if (secs >= TIMEOUT_IN_SECONDS) {
        std::cerr << "timeout" << std::endl;
        return 1;
    }

    return 0;
}

