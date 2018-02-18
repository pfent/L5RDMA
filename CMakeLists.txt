cmake_minimum_required(VERSION 3.0)
enable_testing()
project(exchangableTransports)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release
        CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo."
        FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)
set(WARNINGS "-Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -Wno-terminate")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic ${WARNINGS} -fshow-column -pipe -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ggdb3 -fno-omit-frame-pointer")

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

SET(LINK_LIBRARIES
        ibverbs
        rt
        pthread
        rdmacm
        tbb
        )

include_directories(.. include)

add_executable(bench bench.cpp $<TARGET_OBJECTS:common>)
target_link_libraries(bench ${LINK_LIBRARIES})

add_executable(kv kvbench.cpp $<TARGET_OBJECTS:common>)
target_link_libraries(kv ${LINK_LIBRARIES})

add_executable(parallel benchParallel.cpp $<TARGET_OBJECTS:common>)
target_link_libraries(parallel ${LINK_LIBRARIES})

add_executable(pingBench pingBench.cpp $<TARGET_OBJECTS:common>)
target_link_libraries(pingBench ${LINK_LIBRARIES})

add_executable(randomWriteBench randomWriteBench.cpp $<TARGET_OBJECTS:common>)
target_link_libraries(randomWriteBench ${LINK_LIBRARIES})

file(GLOB tests "test/*Test.cpp")
foreach (test ${tests})
    get_filename_component(testName ${test} NAME_WE)
    add_executable(${testName} ${test} $<TARGET_OBJECTS:common>)
    target_link_libraries(${testName} ${LINK_LIBRARIES})
    add_test(${testName} ${testName})
endforeach ()
