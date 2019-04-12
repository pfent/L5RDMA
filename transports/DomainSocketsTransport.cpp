#include "include/DomainSocketsTransport.h"
#include "util/socket/domain.h"

namespace l5 {
namespace transport {
using namespace util;

DomainSocketsTransportServer::DomainSocketsTransportServer(std::string file) :
        initialSocket(domain::socket()),
        file(std::move(file)) {
    domain::bind(initialSocket, this->file);
    domain::listen(initialSocket);
}

DomainSocketsTransportServer::~DomainSocketsTransportServer() = default;

void DomainSocketsTransportServer::accept_impl() {
    communicationSocket = domain::accept(initialSocket);
}

void DomainSocketsTransportServer::write_impl(const uint8_t *data, size_t size) {
    domain::write(communicationSocket, data, size);
}

void DomainSocketsTransportServer::read_impl(uint8_t *buffer, size_t size) {
    domain::read(communicationSocket, buffer, size);
}

DomainSocketsTransportClient::DomainSocketsTransportClient() : socket(domain::socket()) {}

DomainSocketsTransportClient::~DomainSocketsTransportClient() = default;

void DomainSocketsTransportClient::connect_impl(std::string file) {
    const auto pos = file.find(':');
    const auto whereTo = std::string(file.begin() + pos + 1, file.end());
    domain::connect(socket, whereTo);
    domain::unlink(whereTo);
}

void DomainSocketsTransportClient::write_impl(const uint8_t *data, size_t size) {
    domain::write(socket, data, size);
}

void DomainSocketsTransportClient::read_impl(uint8_t *buffer, size_t size) {
    domain::read(socket, buffer, size);
}
} // namespace transport
} // namespace l5
