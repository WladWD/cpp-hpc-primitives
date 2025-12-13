#include <gtest/gtest.h>

#include <hpc/core/pool_allocator.hpp>

TEST(PoolAllocator, Basic)
{
    hpc::core::fixed_pool pool(sizeof(int), 4);

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    void* p3 = pool.allocate();
    void* p4 = pool.allocate();

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_NE(p3, nullptr);
    EXPECT_NE(p4, nullptr);

    EXPECT_EQ(pool.allocate(), nullptr); // pool exhausted

    pool.deallocate(p2);
    EXPECT_NE(pool.allocate(), nullptr);
}

