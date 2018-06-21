#include "include/LibRdmacmTransport.h"

#include <netdb.h>

namespace l5 {
namespace transport {
LibRdmacmTransportClient::LibRdmacmTransportClient() = default;

LibRdmacmTransportClient::~LibRdmacmTransportClient() {
    if (rdmaSocket > 0) {
        rshutdown(rdmaSocket, SHUT_RDWR);
        rclose(rdmaSocket);
    }
}

void LibRdmacmTransportClient::connect_impl(std::string_view connection) {
    const auto pos = connection.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("usage: <0.0.0.0:port>");
    }
    const auto dst = std::string(connection.data(), pos);
    const auto port = std::string(connection.begin() + pos + 1, connection.end());

    addrinfo hints{};
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *res;

    if (getaddrinfo(dst.c_str(), port.c_str(), &hints, &res)) {
        perror("getaddrinfo");
        throw std::runtime_error{"getaddrinfo failed"};
    }

    rdmaSocket = rsocket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (rdmaSocket < 0) {
        perror("rsocket");
        throw std::runtime_error{"rsocket failed"};
    }
}

void LibRdmacmTransportClient::write_impl(const uint8_t *data, size_t size) {
    rwrite(rdmaSocket, data, size);
}

void LibRdmacmTransportClient::read_impl(uint8_t *buffer, size_t size) {
    rread(rdmaSocket, buffer, size);
}

LibRdmacmTransportServer::LibRdmacmTransportServer(std::string_view port) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo *resPtr;
    auto p = std::string(port);
    if (getaddrinfo(nullptr, p.c_str(), &hints, &resPtr)) {
        perror("getaddrinfo");
        throw std::runtime_error{"getaddrinfo failed"};
    }
    std::unique_ptr<addrinfo> res{resPtr};

    rdmaSocket = rsocket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (rdmaSocket < 0) {
        perror("rsocket");
        throw std::runtime_error{"rsocket failed"};
    }

    if (rbind(rdmaSocket, res->ai_addr, res->ai_addrlen)) {
        perror("rbind");
        throw std::runtime_error{"rbind failed"};
    }

    if (rlisten(rdmaSocket, 1)) {
        perror("rlisten");
        throw std::runtime_error{"rlisten failed"};
    }
}

LibRdmacmTransportServer::~LibRdmacmTransportServer() {
    rshutdown(rdmaSocket, SHUT_RDWR);
    rclose(rdmaSocket);
    if (commSocket > 0) {
        rshutdown(commSocket, SHUT_RDWR);
        rclose(commSocket);
    }
}

void LibRdmacmTransportServer::accept_impl() {
    sockaddr ignored{};
    socklen_t len = sizeof(ignored);
    commSocket = raccept(rdmaSocket, &ignored, &len);
}

void LibRdmacmTransportServer::write_impl(const uint8_t *data, size_t size) {
    rwrite(commSocket, data, size);
}

void LibRdmacmTransportServer::read_impl(uint8_t *buffer, size_t size) {
    rread(commSocket, buffer, size);
}
} // namespace l5
} // namespace transport
