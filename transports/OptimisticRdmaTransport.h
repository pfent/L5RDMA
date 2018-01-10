#ifndef EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H
#define EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H

class OptimisticRdmaTransport {
    // Similar to the usual RdmaTransport, but instead of a single producer single consumer supports multiple producers.
    // In an optimistic way, write to a random position in the buffer, but use a IBV_WR_RDMA_WRITE_WITH_IMM message.
    // The IMM value then contains, where the message is located within the buffer and the receiver can poll the receive
    // queue for the IMM data. When a completion is received, we also know, that the message has already been written
    // completely, so we can even save the few bytes of the stop marker.
    // However this is a probabilistic approach, similar to ALOHA. The sender can't know if the message was received
    // successfully and the receiver needs a way to detect, if there was a collision. Thus the sender needs to calculate
    // a checksum (CRC) and append it to the message
};

#endif //EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H
