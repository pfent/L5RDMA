#include <exchangableTransports/util/domainSocketsWrapper.h>
#include <sys/un.h>
#include "DomainSocketsTransport.h"

DomainSocketsTransportServer::DomainSocketsTransportServer(std::string_view file) : initialSocket(domain_socket()) {
    domain_bind(initialSocket, file);
    domain_listen(initialSocket);
}

DomainSocketsTransportServer::~DomainSocketsTransportServer() {
    domain_close(initialSocket);

    if (communicationSocket != -1) {
        domain_close(communicationSocket);
    }
}

void DomainSocketsTransportServer::accept() {
    sockaddr_un remote{};
    communicationSocket = domain_accept(initialSocket, remote);
}

void DomainSocketsTransportServer::write(const uint8_t *data, size_t size) {
    domain_write(communicationSocket, data, size);
}

void DomainSocketsTransportServer::read(uint8_t *buffer, size_t size) {
    domain_read(communicationSocket, buffer, size);
}

DomainSocketsTransportClient::DomainSocketsTransportClient() : socket(domain_socket()) {}

DomainSocketsTransportClient::~DomainSocketsTransportClient() {
    domain_close(socket);
}

void DomainSocketsTransportClient::connect(std::string_view file) {
    domain_connect(socket, file);
}

void DomainSocketsTransportClient::write(const uint8_t *data, size_t size) {
    domain_write(socket, data, size);
}

void DomainSocketsTransportClient::read(uint8_t *buffer, size_t size) {
    domain_read(socket, buffer, size);
}
