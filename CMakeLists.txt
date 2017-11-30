cmake_minimum_required(VERSION 3.0)
enable_testing()
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
        apps/PingPong.h
        util/bench.h
        util/virtualMemory.cpp
        datastructures/VirtualRingBuffer.cpp
        datastructures/VirtualRDMARingBuffer.cpp
        util/RDMANetworking.cpp
        )

SET(LINK_LIBRARIES
        ibverbs
        rt
        pthread
        )

include_directories(..)

add_executable(bench bench.cpp ${SOURCE_FILES})
target_link_libraries(bench ${LINK_LIBRARIES})

file(GLOB tests "test/*Test.cpp")
foreach (test ${tests})
    get_filename_component(testName ${test} NAME_WE)
    add_executable(${testName} ${test} ${SOURCE_FILES})
    target_link_libraries(${testName} ${LINK_LIBRARIES})
    add_test(${testName} ${testName})
endforeach ()
