cmake_minimum_required(VERSION 3.0)
project(rdma_hash_map)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wnon-virtual-dtor -fshow-column -pipe -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3 -O0 -DDEBUG -fsanitize=undefined -fsanitize=address -fsanitize=leak")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG -fvisibility=hidden")

set(SOURCE_FILES
        rdma/CompletionQueuePair.cpp
        rdma/MemoryRegion.cpp
        rdma/Network.cpp
        rdma/QueuePair.cpp
        rdma/ReceiveQueue.cpp
        rdma/WorkRequest.cpp
        tcpWrapper.cpp
        RDMAMessageBuffer.cpp
        )
set(OVERRIDES_FILES
        fileDescriptorOverrides/overrides.cpp
        fileDescriptorOverrides/realFunctions.cpp)

include_directories(..)

add_executable(minimal minimal.cpp ${SOURCE_FILES})
target_link_libraries(minimal ibverbs)

add_executable(tcpPingPong tcpPingPong.cpp)

add_executable(rdmaPingPong rdmaPingPong.cpp ${SOURCE_FILES})
target_link_libraries(rdmaPingPong ibverbs)

add_library(Test SHARED ${SOURCE_FILES} ${OVERRIDES_FILES})
target_link_libraries(Test ibverbs)
