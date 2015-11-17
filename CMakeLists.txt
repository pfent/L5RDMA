cmake_minimum_required(VERSION 3.3)
project(rdma_hash_map)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

set(SOURCE_FILES Coordinator.cpp Perf.cpp Tester.cpp dht/HashTableClient.cpp dht/HashTableNetworkLayout.cpp dht/HashTableServer.cpp dht/requests/RequestQueue.cpp dht/requests/Request.cpp rdma/CompletionQueuePair.cpp rdma/MemoryRegion.cpp rdma/Network.cpp rdma/QueuePair.cpp rdma/ReceiveQueue.cpp rdma/WorkRequest.cpp util/ConnectionSetup.cpp util/InlineList.cpp util/Utility.cpp)

include_directories(. /usr/local/Cellar/zeromq/4.1.3/include/)
add_executable(SampleCode "${SOURCE_FILES}")