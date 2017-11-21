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

    if (communicationSocket != -1) {
        domain_close(communicationSocket);
    }
}

void SharedMemoryTransportServer::accept_impl() {
    sockaddr_un remote{};
    communicationSocket = domain_accept(initialSocket, remote);

    messageBuffer = std::make_unique<SharedMemoryDatastructure>(BUFFER_SIZE, communicationSocket);
}

void SharedMemoryTransportServer::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportServer::read_impl(uint8_t *buffer, size_t size) {
    messageBuffer->receive(buffer, size);
}

Buffer SharedMemoryTransportServer::getBuffer_impl(size_t size) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void SharedMemoryTransportServer::write_impl(Buffer &buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

Buffer SharedMemoryTransportServer::read_impl(size_t size) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void SharedMemoryTransportServer::markAsRead_impl(Buffer &readBuffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

SharedMemoryTransportClient::SharedMemoryTransportClient() : socket(domain_socket()) {}

SharedMemoryTransportClient::~SharedMemoryTransportClient() {
    domain_close(socket);
}

void SharedMemoryTransportClient::connect_impl(std::string_view file) {
    domain_connect(socket, file);
    domain_unlink(file);

    messageBuffer = std::make_unique<SharedMemoryDatastructure>(BUFFER_SIZE, socket);
}

void SharedMemoryTransportClient::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportClient::read_impl(uint8_t *buffer, size_t size) {
    messageBuffer->receive(buffer, size);
}

Buffer SharedMemoryTransportClient::getBuffer_impl(size_t size) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void SharedMemoryTransportClient::write_impl(Buffer &buffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

void SharedMemoryTransportClient::markAsRead_impl(Buffer &readBuffer) {
    throw std::runtime_error{"not implemented!"}; // TODO
}

Buffer SharedMemoryTransportClient::read_impl(size_t size) {
    throw std::runtime_error{"not implemented!"}; // TODO
}
