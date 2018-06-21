#include "SharedMemoryTransport.h"

#include <sys/un.h>
#include "util/socket/domain.h"

namespace l5 {
namespace transport {
using namespace util;

SharedMemoryTransportServer::SharedMemoryTransportServer(const std::string &domainSocket) :
        initialSocket(domain::socket()),
        file(domainSocket) {
    domain::bind(initialSocket, file);
    domain::listen(initialSocket);
}

SharedMemoryTransportServer::~SharedMemoryTransportServer() = default;

void SharedMemoryTransportServer::accept_impl() {
    communicationSocket = domain::accept(initialSocket);

    messageBuffer = std::make_unique<datastructure::VirtualRingBuffer>(BUFFER_SIZE, communicationSocket);
}

void SharedMemoryTransportServer::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportServer::read_impl(uint8_t *buffer, size_t size) {
    messageBuffer->receive(buffer, size);
}

SharedMemoryTransportClient::SharedMemoryTransportClient() : socket(domain::socket()) {}

SharedMemoryTransportClient::~SharedMemoryTransportClient() = default;

void SharedMemoryTransportClient::connect_impl(const std::string &file) {
    const auto pos = file.find(':');
    const auto whereTo = std::string(file.begin() + pos + 1, file.end());
    domain::connect(socket, whereTo);
    domain::unlink(whereTo);

    messageBuffer = std::make_unique<datastructure::VirtualRingBuffer>(BUFFER_SIZE, socket);
}

void SharedMemoryTransportClient::write_impl(const uint8_t *data, size_t size) {
    messageBuffer->send(data, size);
}

void SharedMemoryTransportClient::read_impl(uint8_t *buffer, size_t size) {
    messageBuffer->receive(buffer, size);
}
} // namespace transport
} // namespace l5
