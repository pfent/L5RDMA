#include "include/LibRdmacmTransport.h"
#include "apps/PingPong.h"
#include <future>
#include <iostream>
#include <sys/wait.h>
#include <zconf.h>

using namespace std;
using namespace l5::transport;

static size_t MESSAGES = 4 * 1024; // ~ 1s
static size_t TIMEOUT_IN_SECONDS = 5;

int main(int, const char** args) {
    if(string(args[0]).find("librdmacmLargeTest") != string::npos) {
        MESSAGES *= 32;
        TIMEOUT_IN_SECONDS *= 32;
    }

    const auto serverPid = fork();
    if (serverPid == 0) {
        auto pong = Pong(make_transportServer<LibRdmacmTransportServer>("1234"));
        pong.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            pong.pong();
        }
        return 0;
    }

    const auto clientPid = fork();
    if (clientPid == 0) {
        sleep(1); // server needs some time to start
        auto ping = Ping(make_transportClient<LibRdmacmTransportClient>(), "127.0.0.1:1234");
        for (size_t i = 0; i < MESSAGES; ++i) {
            ping.ping();
        }
    }

    int serverStatus = 1;
    int clientStatus = 1;
    size_t secs = 0;
    for (; secs < TIMEOUT_IN_SECONDS; ++secs, sleep(1)) {
        auto serverTerminated = waitpid(serverPid, &serverStatus, WNOHANG) != 0;
        auto clientTerminated = waitpid(clientPid, &clientStatus, WNOHANG) != 0;
        if (serverTerminated && clientTerminated) {
            break;
        }
    }

    if (secs >= TIMEOUT_IN_SECONDS) {
        std::cerr << "timeout" << std::endl;
        kill(serverPid, SIGTERM);
        kill(clientPid, SIGTERM);
        return 1;
    }

    return serverStatus + clientStatus;
}
