cmake_minimum_required(VERSION 3.3)
project(rdma_hash_map)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -fshow-column -pipe -march=native")

set(SOURCE_FILES rdma/CompletionQueuePair.cpp rdma/MemoryRegion.cpp rdma/Network.cpp rdma/QueuePair.cpp rdma/ReceiveQueue.cpp rdma/WorkRequest.cpp)

include_directories(..)
add_executable(Test main.cpp "${SOURCE_FILES}")
target_link_libraries(Test pthread ibverbs)

add_executable(minimal minimal.cpp "${SOURCE_FILES}")
target_link_libraries(minimal pthread ibverbs)