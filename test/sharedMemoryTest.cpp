#include <exchangeableTransports/transports/SharedMemoryTransport.h>
#include <exchangeableTransports/apps/PingPong.h>
#include <future>
#include <iostream>
#include <sys/wait.h>

using namespace std;

const size_t MESSAGES = 4 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    const auto serverPid = fork();
    if (serverPid == 0) {
        auto pong = Pong(make_transportServer<SharedMemoryTransportServer>("/tmp/pingPong"));
        pong.start();
        for (size_t i = 0; i < MESSAGES; ++i) {
            pong.pong();
        }
        return 0;
    }

    const auto clientPid = fork();
    if (clientPid == 0) {
        sleep(1); // server needs some time to start
        auto ping = Ping(make_transportClient<SharedMemoryTransportClient>(), "/tmp/pingPong");
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
        return 1;
    }

    return serverStatus + clientStatus;
}

