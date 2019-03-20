#pragma once

#include <memory>
#include "CompletionQueuePair.hpp"

namespace rdma {
    using MemoryRegion = std::unique_ptr<ibv::memoryregion::MemoryRegion>;

    std::ostream &operator<<(std::ostream &os, const ibv::memoryregion::RemoteAddress &remoteMemoryRegion);

    /// The LID and QPN uniquely address a queue pair
    struct Address {
        ibv::Gid gid;
        uint32_t qpn;
        uint16_t lid;
    };

    std::ostream &operator<<(std::ostream &os, const Address &address);

    /// Abstracts a global rdma context
    class Network {
        friend class QueuePair;

        static constexpr uint32_t maxWr = 16351;
        static constexpr uint32_t maxSge = 1;

        /// The port of the Infiniband device
        static constexpr uint8_t ibport = 1;

        /// The Infiniband devices
        ibv::device::DeviceList devices;
        /// The verbs context
        std::unique_ptr<ibv::context::Context> context;
        /// The global protection domain
        std::unique_ptr<ibv::protectiondomain::ProtectionDomain> protectionDomain;

        /// Shared Queues
        CompletionQueuePair sharedCompletionQueuePair;

        std::unique_ptr<ibv::srq::SharedReceiveQueue> sharedReceiveQueue;

    public:
        Network();

        /// Get the LID
        uint16_t getLID();

        /// Get the GID
        ibv::Gid getGID();

        /// Print the capabilities of the RDMA host channel adapter
        void printCapabilities();

        CompletionQueuePair newCompletionQueuePair();

        CompletionQueuePair &getSharedCompletionQueue();

        /// Register a new MemoryRegion
        std::unique_ptr<ibv::memoryregion::MemoryRegion>
        registerMr(void *addr, size_t length, std::initializer_list<ibv::AccessFlag> flags);

        ibv::protectiondomain::ProtectionDomain& getProtectionDomain();
    };
}
