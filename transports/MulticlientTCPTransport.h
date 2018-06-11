#ifndef L5RDMA_MULTICLIENTTCPTRANSPORT_H
#define L5RDMA_MULTICLIENTTCPTRANSPORT_H

#include <string_view>
#include <vector>
#include <poll.h>

class MulticlientTCPTransportServer {
    const int serverSocket;
    std::vector<int> connections;
    std::vector<pollfd> pollFds;

    void listen(uint16_t port);

public:
    explicit MulticlientTCPTransportServer(std::string_view port);

    ~MulticlientTCPTransportServer();

    void accept();

    size_t receive(void *whereTo, size_t maxSize);

    void send(size_t receiverId, const uint8_t *data, size_t size);

    template<typename TriviallyCopyable>
    void write(size_t receiverId, const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable_v<TriviallyCopyable>);
        send(receiverId, reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    template<typename TriviallyCopyable>
    size_t read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable_v<TriviallyCopyable>);
        return receive(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }
};

class MulticlientTCPTransportClient {
    const int socket;
public:
    MulticlientTCPTransportClient();

    ~MulticlientTCPTransportClient();

    void connect(std::string_view whereTo);

    void connect(const std::string &ip, uint16_t port);

    void send(const uint8_t *data, size_t size);

    void receive(void *whereTo, size_t maxSize);

    template<typename TriviallyCopyable>
    void write(const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable_v<TriviallyCopyable>);
        send(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    template<typename TriviallyCopyable>
    void read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable_v<TriviallyCopyable>);
        receive(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }
};


#endif //L5RDMA_MULTICLIENTTCPTRANSPORT_H
