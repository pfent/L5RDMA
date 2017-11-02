#ifndef EXCHANGABLETRANSPORTS_TRANSPORT_H
#define EXCHANGABLETRANSPORTS_TRANSPORT_H

#include <string_view>
#include "Buffer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * Dummy class used to specify the Transport interface
 * Transports abstract from sockets as data communication to also provide mechanisms, that are useful to send data via
 * shared memory
 */
class TransportServer {
public:
    /**
     * Creates a server, listening on the specified incoming specifier
     * @param incoming where clients arrive in string representation; this can be a port,
     *                 e.g. "1234" or a socket "/tmp/asdf"
     */
    explicit TransportServer(std::string_view incoming) { throw; };

    /**
     * Accept a new connection from the remote end.
     * Right now, this only allows exactly one connection. This might change in the future, returning an identifier
     */
    void accept() { throw; };

    /**
     * Get a new Buffer, where data can be written to. This is useful to provide zero copy mechanisms, reducing latency
     * @see write(Buffer&)
     * @param size the size for the new buffer
     */
    Buffer getBuffer(size_t size) { throw; };

    /**
     * Send data from a buffer previously acquired with getBuffer(size_t)
     * @param buffer the buffer
     */
    void write(Buffer &buffer) { throw; };

    /**
     * Get the buffer, where the latest message has been written to. This is useful from zero copy mechanisms
     */
    Buffer read() { throw; };

    /**
     * Mark a buffer from read() as finished.
     */
    void markAsRead(Buffer &readBuffer) { throw; };

    /**
     * Send data from an arbitrary memory location
     */
    void write(uint8_t *buffer, size_t size) { throw; };

    /**
     * Receive data to an arbitrary memory location
     */
    void read(uint8_t *whereTo, size_t size) { throw; };
};

class TransportClient {
public:

    TransportClient() { throw; };

    /**
     * Connect to a remote TransportServer, as specified in whereTo
     * TODO: find a common interface
     */
    void connect(std::string_view whereTo) { throw; };

    /**
     * Similar interface to TransportServer
     */

    Buffer getBuffer(size_t size) { throw; };

    void write(Buffer &buffer) { throw; };

    Buffer read() { throw; };

    void markAsRead(Buffer &readBuffer) { throw; };

    void write(uint8_t *buffer, size_t size) { throw; };

    void read(uint8_t *whereTo, size_t size) { throw; };
};

#pragma GCC diagnostic pop
#endif //EXCHANGABLETRANSPORTS_TRANSPORT_H
