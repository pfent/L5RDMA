#ifndef EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
#define EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <unistd.h>

class VirtualRDMARingBuffer {
    const size_t size;
    const size_t bitmask;

    std::shared_ptr<RingBufferInfo> localRw;
    std::shared_ptr<uint8_t> local1;
    std::shared_ptr<uint8_t> local2; // safeguarding virtual memory region, using the MMU for wraparound

    std::shared_ptr<RingBufferInfo> remoteRw;
    std::shared_ptr<uint8_t> remote1;
    std::shared_ptr<uint8_t> remote2; // safeguarding virtual memory region, using the MMU for wraparound
};

#endif //EXCHANGABLETRANSPORTS_VIRTUALRDMARINGBUFFER_H
