#include <exchangeableTransports/transports/DomainSocketsTransport.h>
#include "pingPongTest.h"

using namespace std;

const size_t MESSAGES = 256 * 1024; // ~ 1s
const size_t TIMEOUT_IN_SECONDS = 5;

int main() {
    return pingPongTest<
            DomainSocketsTransportServer,
            DomainSocketsTransportClient
    >(MESSAGES, TIMEOUT_IN_SECONDS);
}

