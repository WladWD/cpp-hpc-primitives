add_executable(hpc_benchmarks
    bench_queue.cpp
    bench_allocator.cpp
    bench_spinlock.cpp
    bench_mpmc_ring_buffer.cpp
    bench_numa_hugepages.cpp
)

target_link_libraries(hpc_benchmarks PRIVATE
    hpc_core
    benchmark::benchmark_main
)

# Favor performance for benchmarks
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hpc_benchmarks PRIVATE -O3 -DNDEBUG -march=native)
endif()
