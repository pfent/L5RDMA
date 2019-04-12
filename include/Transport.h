#pragma once

#include <memory>
#include <string>

namespace l5 {
namespace transport {
/**
 * CRTP class used to specify the Transport interface via static polymorphism
 * Transports abstract from sockets as data communication to also provide mechanisms, that are useful to send data via
 * shared memory
 */
template<class T>
class TransportServer {
public:

    /**
     * Accept a new connection from the remote end.
     * Right now, this only allows exactly one connection. This might change in the future, returning an identifier
     */
    void accept() { static_cast<T *>(this)->accept_impl(); }

    /**
     * Send data from an arbitrary memory location
     */
    void write(const uint8_t *buffer, size_t size) { static_cast<T *>(this)->write_impl(buffer, size); }

    template<typename TriviallyCopyable>
    void write(const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        write(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    /**
     * Receive data to an arbitrary memory location
     */
    void read(uint8_t *whereTo, size_t size) { static_cast<T *>(this)->read_impl(whereTo, size); }

    template<typename TriviallyCopyable>
    void read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        read(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }

    template<typename TriviallyCopyable>
    TriviallyCopyable read() {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        TriviallyCopyable data;
        read(reinterpret_cast<uint8_t *>(&data), sizeof(data));
        return data;
    }

    virtual ~TransportServer() = default;
};

template<class T>
class TransportClient {
public:

    /**
     * Connect to a remote TransportServer, as specified in whereTo
     */
    void connect(const std::string &whereTo) { static_cast<T *>(this)->connect_impl(whereTo); }

    /**
     * Reset the connection to reuse resources and connect again
     */
    void reset() { static_cast<T *>(this)->reset_impl(); }

    /**
     * Similar interface to TransportServer
     */
    void write(const uint8_t *buffer, size_t size) { static_cast<T *>(this)->write_impl(buffer, size); }

    template<typename TriviallyCopyable>
    void write(const TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        write(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    void read(uint8_t *whereTo, size_t size) { static_cast<T *>(this)->read_impl(whereTo, size); }

    template<typename TriviallyCopyable>
    void read(TriviallyCopyable &data) {
        static_assert(std::is_trivially_copyable<TriviallyCopyable>::value, "");
        read(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }

    virtual ~TransportClient() = default;
};

template<typename Derived, typename... Args>
std::unique_ptr<TransportServer<Derived>> make_transportServer(Args &&... args) {
    return std::move(std::make_unique<Derived>(std::forward<Args>(args)...));
}

template<typename Derived, typename... Args>
std::unique_ptr<TransportClient<Derived>> make_transportClient(Args &&... args) {
    return std::move(std::make_unique<Derived>(std::forward<Args>(args)...));
}
} // namespace transport
} // namespace l5
