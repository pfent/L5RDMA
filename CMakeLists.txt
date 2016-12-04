cmake_minimum_required(VERSION 3.3)
project(rdma_hash_map)
# needed dependencies: zeromq and libibverbs

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -lpthread -libverbs")

set(SOURCE_FILES rdma/CompletionQueuePair.cpp rdma/MemoryRegion.cpp rdma/Network.cpp rdma/QueuePair.cpp rdma/ReceiveQueue.cpp rdma/WorkRequest.cpp main.cpp)

include_directories(.)
add_executable(Test main.cpp "${SOURCE_FILES}")
add_executable(minimal minimal.cpp "${SOURCE_FILES}")