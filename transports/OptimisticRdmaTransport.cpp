#include <netinet/in.h>
#include "OptimisticRdmaTransport.h"

OptimisticRdmaTransportServer::OptimisticRdmaTransportServer(std::string_view port) :
        sock(tcp_socket()), net() {
    auto p = std::stoi(std::string(port.data(), port.size()));
    listen(p);
}

void OptimisticRdmaTransportServer::listen(uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(sock, addr);
    tcp_listen(sock);
}
