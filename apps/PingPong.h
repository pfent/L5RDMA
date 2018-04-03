#ifndef EXCHANGABLETRANSPORTS_PINGPONG_H
#define EXCHANGABLETRANSPORTS_PINGPONG_H

#include <cstdint>
#include <vector>
#include "transports/Transport.h"

using namespace std::string_view_literals;

template<class T>
class Ping {
    std::vector<uint8_t> data;
    std::unique_ptr<TransportClient<T>> transport;
    std::vector<uint8_t> buffer;
public:
    Ping(std::unique_ptr<TransportClient<T>> t, std::string_view whereTo, size_t dataSize = 64)
            : transport(std::move(t)),
              buffer(dataSize) {
        transport->connect(whereTo);
        for (size_t i = 0; i < dataSize; ++i) {
            data.push_back(i % 255);
        }
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
};

template<class T>
class Pong {
    std::unique_ptr<TransportServer<T>> transport;
    std::vector<uint8_t> buffer;
public:
    explicit Pong(std::unique_ptr<TransportServer<T>> transport, size_t dataSize = 64)
            : transport(std::move(transport)),
              buffer(dataSize) {}

    void start() {
        transport->accept();
    }

    void pong() {
        transport->read(buffer.data(), buffer.size());
        transport->write(buffer.data(), buffer.size());
    }
};

#endif //EXCHANGABLETRANSPORTS_PINGPONG_H
