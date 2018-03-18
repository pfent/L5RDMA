#ifndef EXCHANGABLETRANSPORTS_NETWORKEXCEPTION_H
#define EXCHANGABLETRANSPORTS_NETWORKEXCEPTION_H

#include <stdexcept>

namespace rdma {
    /// A network exception
    struct NetworkException : public std::runtime_error {
        using std::runtime_error::runtime_error;

        ~NetworkException() override;
    };
}

#endif //EXCHANGABLETRANSPORTS_NETWORKEXCEPTION_H
