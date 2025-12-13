cmake_minimum_required(VERSION 3.20)
project(cpp-hpc-primitives VERSION 0.1.0 LANGUAGES CXX)

option(HPC_ENABLE_TESTS "Build unit tests" ON)
option(HPC_ENABLE_BENCHMARKS "Build benchmarks" ON)
option(HPC_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

add_library(hpc_core
    src/arena_allocator.cpp
    src/pool_allocator.cpp
    src/ipc/shm_ring_buffer.cpp
    src/support/cpu_topology.cpp
    src/support/clock.cpp
)

hpc_enable_strict_warnings(hpc_core)

target_include_directories(hpc_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(hpc_core PROPERTIES
    OUTPUT_NAME "hpc_core"
    POSITION_INDEPENDENT_CODE ON
)

if(HPC_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HPC_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
