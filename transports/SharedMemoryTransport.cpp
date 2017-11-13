#include "SharedMemoryTransport.h"

#include <sys/un.h>
#include <exchangeableTransports/util/domainSocketsWrapper.h>

SharedMemoryTransportServer::SharedMemoryTransportServer(std::string_view domainSocket) :
        initialSocket(domain_socket()),
        file(domainSocket) {
    domain_bind(initialSocket, file);
    domain_listen(initialSocket);
}

SharedMemoryTransportServer::~SharedMemoryTransportServer() {
    // close socket
    domain_close(initialSocket);
    domain_unlink(file);

    if (communicationSocket != -1) {
        domain_close(communicationSocket);
    }
}

void SharedMemoryTransportServer::accept_impl() {
    sockaddr_un remote{};
    communicationSocket = domain_accept(initialSocket, remote);

    messageBuffer = std::make_unique<SharedMemoryMessageBuffer>(BUFFER_SIZE, communicationSocket); // 10MB
}

void SharedMemoryTransportServer::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportServer::read_impl(uint8_t *buffer, size_t size) {
    messageBuffer->receive(buffer, size);
}

SharedMemoryTransportClient::SharedMemoryTransportClient() : socket(domain_socket()) {}

SharedMemoryTransportClient::~SharedMemoryTransportClient() {
    domain_close(socket);
}

void SharedMemoryTransportClient::connect_impl(std::string_view file) {
    domain_connect(socket, file);
    domain_unlink(file);

    messageBuffer = std::make_unique<SharedMemoryMessageBuffer>(BUFFER_SIZE, socket); // 10MB
}

void SharedMemoryTransportClient::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportClient::read_impl(uint8_t *buffer, size_t size) {

}