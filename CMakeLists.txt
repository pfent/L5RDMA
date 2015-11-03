cmake_minimum_required(VERSION 3.3)
project(rdma_hash_map)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES Coordinator.cpp Perf.cpp Tester.cpp dht/HashTableClient.cpp dht/HashTableNetworkLayout.cpp dht/HashTableServer.cpp dht/requests/RequestQueue.cpp rdma/CompletionQueuePair.cpp rdma/MemoryRegion.cpp rdma/Network.cpp rdma/QueuePair.cpp rdma/ReceiveQueue.cpp rdma/WorkRequest.cpp util/ConnectionSetup.cpp util/FreeListAllocator.cpp util/Utility.cpp)

add_executable(SampleCode "${SOURCE_FILES}")