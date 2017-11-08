cmake_minimum_required(VERSION 3.0)
project(exchangableTransports)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -fshow-column -pipe -march=native")
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
        datastructures/RDMAMessageBuffer.cpp
        )
set(OVERRIDES_FILES
        fileDescriptorOverrides/overrides.cpp
        fileDescriptorOverrides/realFunctions.cpp)

include_directories(..)

add_executable(pingPong pingPong.cpp util/tcpWrapper.cpp transports/TcpTransport.cpp transports/RdmaTransport.cpp ${SOURCE_FILES} transports/DomainSocketsTransport.cpp transports/DomainSocketsTransport.h datastructures/SharedMemoryMessageBuffer.cpp datastructures/SharedMemoryMessageBuffer.h util/domainSocketsWrapper.cpp util/domainSocketsWrapper.h transports/Transport.h transports/Buffer.h transports/SharedMemoryTransport.h transports/SharedMemoryTransport.cpp)
target_link_libraries(pingPong ibverbs rt)
