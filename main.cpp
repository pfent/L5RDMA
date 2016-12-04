#include "rdma/Network.hpp"

using namespace rdma;
int main() {
    Network net{};
    net.printCapabilities();
    return 0;
}

