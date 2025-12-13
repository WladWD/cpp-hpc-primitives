#include <gtest/gtest.h>

#include <hpc/core/arena_allocator.hpp>

TEST(ArenaAllocator, Basic)
{
    hpc::core::arena a(1024);

    void* p1 = a.allocate(16, alignof(int));
    ASSERT_NE(p1, nullptr);

    void* p2 = a.allocate(16, alignof(int));
    ASSERT_NE(p2, nullptr);

    EXPECT_LT(p1, p2);

    a.reset();

    void* p3 = a.allocate(1024, alignof(int));
    EXPECT_NE(p3, nullptr);
}

