#pragma once

#include <mutex>
#include <stdexcept>
#include <vector>
#include <memory>
#include <libibverbscpp/libibverbscpp.h>

namespace rdma {
    struct CompletionQueuePair;

    using MemoryRegion = std::unique_ptr<ibv::memoryregion::MemoryRegion>;

    /// A network exception
    struct NetworkException : public std::runtime_error {
        explicit NetworkException(const std::string &reason)
                : std::runtime_error(reason) {}
    };

    struct RemoteMemoryRegion {
        uintptr_t address;
        uint32_t key;

        RemoteMemoryRegion slice(size_t offset);
    };

    std::ostream &operator<<(std::ostream &os, const RemoteMemoryRegion &remoteMemoryRegion);

    /// The LID and QPN uniquely address a queue pair
    struct Address {
        uint32_t qpn;
        uint16_t lid;
    };

    std::ostream &operator<<(std::ostream &os, const Address &address);

    /// Abstracts a global rdma context
    class Network {
        friend class CompletionQueuePair;

        friend class QueuePair;

        /// The minimal number of entries for the completion queue
        static const int CQ_SIZE = 100;

        /// The port of the Infiniband device
        uint8_t ibport = 1;

        /// The Infiniband devices
        ibv::device::DeviceList devices;
        /// The verbs context
        std::unique_ptr<ibv::context::Context> context;
        /// The global protection domain
        std::unique_ptr<ibv::protectiondomain::ProtectionDomain> protectionDomain;

        /// Shared Queues
        std::unique_ptr<CompletionQueuePair> sharedCompletionQueuePair;

        std::unique_ptr<ibv::srq::SharedReceiveQueue> sharedReceiveQueue;

    public:
        /// Constructor
        Network();

        /// Get the LID
        uint16_t getLID();

        /// Print the capabilities of the RDMA host channel adapter
        void printCapabilities();

        /// Register a new MemoryRegion
        std::unique_ptr<ibv::memoryregion::MemoryRegion>
        registerMr(void *addr, size_t length, std::initializer_list<ibv::AccessFlag> flags);
    };
}
