#ifndef EXCHANGABLETRANSPORTS_TRANSPORT_H
#define EXCHANGABLETRANSPORTS_TRANSPORT_H

#include <string_view>
#include <memory>
#include "Buffer.h"


// todo: mtcp transport / dpdk, MSG_ZEROCOPY kernel interface https://lwn.net/Articles/726917/


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
    void accept() { static_cast<T *>(this)->accept_impl(); };

    /**
     * Get a new Buffer, where data can be written to. This is useful to provide zero copy mechanisms, reducing latency
     * @see write(Buffer&)
     * @param size the size for the new buffer
     */
    Buffer getBuffer(size_t size) { return static_cast<T *>(this)->getBuffer_impl(size); };

    /**
     * Send data from a buffer previously acquired with getBuffer(size_t)
     * @param buffer the buffer
     */
    void write(Buffer &buffer) { static_cast<T *>(this)->write_impl(buffer); };

    /**
     * Get the buffer, where the latest message has been written to. This is useful from zero copy mechanisms
     */
    Buffer read(size_t size) { return static_cast<T *>(this)->read_impl(size); };

    /**
     * Mark a buffer from read() as finished.
     */
    void markAsRead(Buffer &readBuffer) { static_cast<T *>(this)->markAsRead_impl(readBuffer); };

    /**
     * Send data from an arbitrary memory location
     */
    void write(uint8_t *buffer, size_t size) { static_cast<T *>(this)->write_impl(buffer, size); };

    /**
     * Receive data to an arbitrary memory location
     */
    void read(uint8_t *whereTo, size_t size) { static_cast<T *>(this)->read_impl(whereTo, size); };

    virtual ~TransportServer() = default;
};

template<class T>
class TransportClient {
public:

    /**
     * Connect to a remote TransportServer, as specified in whereTo
     */
    void connect(std::string_view whereTo) { static_cast<T *>(this)->connect_impl(whereTo); };

    /**
     * Similar interface to TransportServer
     */

    Buffer getBuffer(size_t size) { return static_cast<T *>(this)->getBuffer_impl(size); };

    virtual void write(Buffer &buffer) { static_cast<T *>(this)->write_impl(buffer); };

    virtual Buffer read(size_t size) { return static_cast<T *>(this)->read_impl(size); };

    virtual void markAsRead(Buffer &readBuffer) { static_cast<T *>(this)->markAsRead_impl(readBuffer); };

    void write(const uint8_t *buffer, size_t size) { static_cast<T *>(this)->write_impl(buffer, size); };

    void read(uint8_t *whereTo, size_t size) { static_cast<T *>(this)->read_impl(whereTo, size); };

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

#endif //EXCHANGABLETRANSPORTS_TRANSPORT_H
