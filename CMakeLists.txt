cmake_minimum_required(VERSION 3.0)
enable_testing()
project(exchangableTransports) # TODO: rename to L5RDMA: Low level, low latency library for RDMA

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release
            CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo."
            FORCE)
ENDIF (NOT CMAKE_BUILD_TYPE)

add_subdirectory(include/benchmark)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_COMPILER clang++)
#set(WARNINGS "-Weverything -Wno-c++98-compat -Wno-shadow-field-in-constructor -Wno-documentation-unknown-command -Wno-shadow -Wno-padded")
set(WARNINGS "-Wall -Wextra -Wnon-virtual-dtor -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict")
set(WARNINGS "${WARNINGS} -Wnull-dereference -Wold-style-cast -Wuseless-cast -Wdouble-promotion")
set(WARNINGS "${WARNINGS} -Wformat=2 -Wshadow=local -Wodr")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic ${WARNINGS} -fshow-column -pipe -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -fno-omit-frame-pointer")

file(GLOB COMMON_SOURCES
        apps/*.cpp
        datastructures/*.cpp
        rdma/*.cpp
        transports/*.cpp
        util/*.cpp
        )
file(GLOB COMMON_HEADERS
        apps/*.h
        datastructures/*.h
        rdma/*.h
        transports/*.h
        util/*.h
        )

add_library(common OBJECT ${COMMON_SOURCES} ${COMMON_HEADERS})

include_directories(.)
include_directories(SYSTEM
        include
        include/benchmark/include
        )

SET(LINK_LIBRARIES
        ibverbs
        rt
        pthread
        rdmacm
        tbb
        benchmark
        )

SET(EXECUTABLES
        point2PointBench
        parallelP2PBench
        rawCommandBench
        indirectWriteBench
        multiclientPollingBench
        many2OneBench
        zeroCopyBench
        ycsbWorkloadCBench
        manySlowSendersBench
        )

foreach (exe ${EXECUTABLES})
    add_executable(${exe} ${exe}.cpp $<TARGET_OBJECTS:common>)
    target_link_libraries(${exe} ${LINK_LIBRARIES})
endforeach ()

file(GLOB tests "test/*Test.cpp")
foreach (test ${tests})
    get_filename_component(testName ${test} NAME_WE)
    add_executable(${testName} ${test} $<TARGET_OBJECTS:common>)
    target_link_libraries(${testName} ${LINK_LIBRARIES})
    add_test(${testName} ${testName})
endforeach ()
