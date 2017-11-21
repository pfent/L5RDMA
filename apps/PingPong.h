#ifndef EXCHANGABLETRANSPORTS_PINGPONG_H
#define EXCHANGABLETRANSPORTS_PINGPONG_H

#include <cstdint>
#include <exchangeableTransports/transports/Transport.h>

using namespace std::string_view_literals;

template<class T>
class Ping {
    static constexpr std::string_view data = "123456789012345678901234567890123456789012345678901234567890123\0"sv;
    std::unique_ptr<TransportClient<T>> transport;
    std::array<uint8_t, data.size()> buffer;
public:
    explicit Ping(std::unique_ptr<TransportClient<T>> transport, std::string_view whereTo) : transport(
            std::move(transport)) {
        this->transport->connect(whereTo);
    }

    void ping() {
        transport->write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
        std::fill(begin(buffer), end(buffer), 0);
        transport->read(buffer.data(), buffer.size());
        for (size_t i = 0; i < data.size(); ++i) {
            if (buffer[i] != data[i]) {
                throw std::runtime_error{"received unexpected data: " + std::string(begin(buffer), end(buffer))};
            }
        }
    }

    void pingZeroCopy() {
        auto buf = transport->getBuffer(data.size());
        std::copy(data.begin(), data.end(), buf.ptr);
        transport->write(std::move(buf));

        auto res = transport->read(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            if (res.ptr[i] != data[i]) {
                throw std::runtime_error{"received unexpected data: " + std::string(res.ptr, &res.ptr[data.size()])};
            }
        }
        transport->markAsRead(std::move(res));
    }
};

template<class T, size_t messageSize = 64>
class Pong {
    std::unique_ptr<TransportServer<T>> transport;
    std::array<uint8_t, messageSize> buffer;
public:
    explicit Pong(std::unique_ptr<TransportServer<T>> transport) : transport(std::move(transport)) {};

    void start() {
        transport->accept();
    }

    void pong() {
        transport->read(buffer.data(), buffer.size());
        transport->write(buffer.data(), buffer.size());
    }

    void pongZeroCopy() {
        auto read = transport->read(messageSize);
        auto send = transport->getBuffer(messageSize);
        std::copy(read.ptr, &read.ptr[messageSize], send.ptr);
        transport->markAsRead(std::move(read));
        transport->write(std::move(send));
    }
};

#endif //EXCHANGABLETRANSPORTS_PINGPONG_H
