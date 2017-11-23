cmake_minimum_required(VERSION 3.0)
project(exchangableTransports)

set(CMAKE_CXX_STANDARD 17)
set(WARNINGS "-Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -Wno-terminate")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic ${WARNINGS} -fshow-column -pipe -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG")

set(SOURCE_FILES
        rdma/CompletionQueuePair.cpp
        rdma/MemoryRegion.cpp
        rdma/Network.cpp
        rdma/QueuePair.cpp
        rdma/ReceiveQueue.cpp
        rdma/WorkRequest.cpp
        util/tcpWrapper.cpp
        util/domainSocketsWrapper.cpp
        datastructures/RDMAMessageBuffer.cpp
        datastructures/SharedMemoryMessageBuffer.cpp
        datastructures/SharedMemoryMessageQueue.cpp
        transports/Transport.h
        transports/Buffer.h
        transports/TcpTransport.cpp
        transports/RdmaTransport.cpp
        transports/DomainSocketsTransport.cpp
        transports/SharedMemoryTransport.cpp
        apps/PingPong.h util/bench.h util/sharedMemory.h datastructures/VirtualRingBuffer.h datastructures/VirtualRingBuffer.cpp datastructures/VirtualRDMARingBuffer.cpp datastructures/VirtualRDMARingBuffer.h util/RDMANetworking.cpp util/RDMANetworking.h)
set(OVERRIDES_FILES
        fileDescriptorOverrides/overrides.cpp
        fileDescriptorOverrides/realFunctions.cpp)

include_directories(..)

add_executable(pingPong pingPong.cpp ${SOURCE_FILES})
target_link_libraries(pingPong ibverbs rt)
