#include <exchangeableTransports/transports/SharedMemoryTransport.h>
#include "pingPongTest.h"

using namespace std;

const size_t MESSAGES = 4 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    return pingPongTest<
            SharedMemoryTransportServer,
            SharedMemoryTransportClient
    >(MESSAGES, TIMEOUT_IN_SECONDS);
}

