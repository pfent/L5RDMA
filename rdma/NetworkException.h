#ifndef L5RDMA_NETWORKEXCEPTION_H
#define L5RDMA_NETWORKEXCEPTION_H

#include <stdexcept>

namespace rdma {
    /// A network exception
    struct NetworkException : public std::runtime_error {
        using std::runtime_error::runtime_error;

        ~NetworkException() override;
    };
}

#endif //L5RDMA_NETWORKEXCEPTION_H
