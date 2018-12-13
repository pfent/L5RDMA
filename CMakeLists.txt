cmake_minimum_required(VERSION 3.0)
enable_testing()
project(L5RDMA CXX)

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release
            CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo."
            FORCE)
ENDIF (NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_COMPILER clang++)
#set(WARNINGS "-Weverything -Wno-c++98-compat -Wno-shadow-field-in-constructor -Wno-documentation-unknown-command -Wno-shadow -Wno-padded")
set(WARNINGS "-Wall -Wextra -Wnon-virtual-dtor -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict")
set(WARNINGS "${WARNINGS} -Wnull-dereference -Wold-style-cast -Wuseless-cast -Wdouble-promotion")
set(WARNINGS "${WARNINGS} -Wformat=2 -Wshadow=local -Wodr")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic ${WARNINGS} -fshow-column -pipe -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=leak")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -fno-omit-frame-pointer")

file(GLOB COMMON_SOURCES
        datastructures/*.cpp
        rdma/*.cpp
        transports/*.cpp
        util/*.cpp
        util/socket/*.cpp
        )
file(GLOB COMMON_HEADERS
        datastructures/*.h
        rdma/*.h
        transports/*.h
        util/*.h
        util/socket/*.h
        )

SET(LINK_LIBRARIES
        ibverbs
        rt
        pthread
        rdmacm
        tbb
        )

add_library(l5rdma-common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
target_include_directories(l5rdma-common PUBLIC .)
target_include_directories(l5rdma-common SYSTEM PUBLIC ext)
target_link_libraries(l5rdma-common ${LINK_LIBRARIES})

SET(EXECUTABLES
        p2pBench
        point2PointBench
        parallelP2PBench
        rawCommandBench
        indirectWriteBench
        multiclientPollingBench
        many2OneBench
        zeroCopyBench
        ycsbWorkloadCBench
        manySlowSendersBench
        ycsbBandwidthBench
        bandwidthBench
        bufferBandwidthBench
        blockedBandwidthBench
        )

foreach (exe ${EXECUTABLES})
    add_executable(${exe} ${exe}.cpp)
    target_link_libraries(${exe} ${LINK_LIBRARIES})
    target_include_directories(${exe} PRIVATE .)
endforeach ()

file(GLOB tests "test/*Test.cpp")
foreach (test ${tests})
    get_filename_component(testName ${test} NAME_WE)
    add_executable(${testName} ${test})
    target_link_libraries(${testName} ${LINK_LIBRARIES} l5rdma-common)
    target_include_directories(${testName} PRIVATE .)
    add_test(${testName} ${testName})
endforeach ()
