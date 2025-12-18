add_executable(hpc_tests
    test_ring_buffer_basic.cpp
    test_arena_allocator.cpp
    test_pool_allocator.cpp
    test_ttas_spinlock.cpp
    test_mpmc_ring_buffer.cpp
    test_huge_pages.cpp
    test_numa_memory.cpp
)

target_link_libraries(hpc_tests PRIVATE
    hpc_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hpc_tests)
