#ifndef EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H
#define EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H

#include <exchangeableTransports/util/virtualMemory.h>
#include <exchangeableTransports/util/RDMANetworking.h>
#include "Transport.h"

/**
  * Based on Victors idea:
  * A single consumer, multi producer message queue, which allows ultra-low latency messages in the common
  * optimistic case.
  *
  * Basic idea: Write to a random position in the buffer and tell the receiver about the position out-of-bounds
  *
  * @link https://github.com/jcxue/RDMA-Tutorial/wiki
  * One approach would be to periodically poll a couple of slots, either single-threaded or with multiple handler
  * threads. However Jiachen Xue writes, that "Neither seems to be efficient or scalable"
  *
  * The most promising approach seems to be sending with `IBV_WR_RDMA_WRITE_WITH_IMM`.
  * This work request (in contrast to the normal one) then generates a completion event in the receive completion
  * queue with an `uint32_t imm_data` field, which should be more than enough to encode the slot.
  * Benefit: `validity` isn't needed anymore
  *
  * The IMM value then contains, where the message is located within the buffer and the receiver can poll the
  * receive queue for the IMM data. When a completion is received, we also know, that the message has already been
  * written completely.
  *
  * This is a probabilistic approach, similar to ALOHA. The sender can't know if the message was received
  * successfully and the receiver needs a way to detect, if there was a collision. Thus the sender needs to
  * calculate a checksum (CRC) and append it to the message
  *
  * Another idea:
  * (16bit x 4B slots + 16bit length - imm_data encoding could be used to reduce message overhead)
  *
  * Back channel:
  * Server has a small-ish answer buffer, with already prepared WriteWorkRequests. Always sending from offset 0,
  * only the length needs to be set each time and the buffer doesn't need to be cleared.
  */
class OptimisticRdmaTransportServer : public TransportServer<OptimisticRdmaTransportServer> {
    const size_t size = 16 * 1024 * 1024;
    const size_t bitmask = size - 1;
    const int sock;
    rdma::Network net;

    /// A separate QueuePair per potential remote writer, as RC queues can't be shared TODO: [citation needed]
    std::vector<rdma::QueuePair> qp;

    WraparoundBuffer receiveBuf;
    rdma::MemoryRegion localReceiveMr;

    std::vector<uint8_t> sendBuf;
    rdma::MemoryRegion localSendBufMr;

    std::vector<rdma::RemoteMemoryRegion> remoteReceiveRmrs;
    std::vector<ibv::workrequest::SendWr> answerWorkRequests;

    void listen(uint16_t port);

public:
    OptimisticRdmaTransportServer(std::string_view port);

    ~OptimisticRdmaTransportServer() override = default;

    void accept_impl();

    void write_impl(const uint8_t *data, size_t size);

    void read_impl(uint8_t *buffer, size_t size);

    Buffer getBuffer_impl(size_t size);

    void write_impl(Buffer buffer);

    Buffer read_impl(size_t size);

    void markAsRead_impl(Buffer readBuffer);
};

class OptimisticRdmaTransportClient : public TransportClient<OptimisticRdmaTransportClient> {
    const size_t size;
    const size_t bitmask;
    RDMANetworking net;

    WraparoundBuffer receiveBuf;
    rdma::MemoryRegion localReceiveMr;

    // TODO
public:
    OptimisticRdmaTransportClient();

    ~OptimisticRdmaTransportClient() override = default;
};

#endif //EXCHANGABLETRANSPORTS_OPTIMISTICRDMATRANSPORT_H
