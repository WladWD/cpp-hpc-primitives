#include <gtest/gtest.h>

#include <hpc/core/numa_arena.hpp>
#include <hpc/core/numa_pool.hpp>

namespace {

TEST(NumaArena, BasicAllocateReset)
{
    constexpr std::size_t kSize = 1 << 16;
    hpc::core::numa_arena arena{kSize, -1}; // portable: no specific node

    void* p1 = arena.allocate(64, alignof(std::max_align_t));
    EXPECT_NE(p1, nullptr);

    void* p2 = arena.allocate(128, alignof(std::max_align_t));
    EXPECT_NE(p2, nullptr);

    EXPECT_LE(arena.capacity(), kSize);

    arena.reset();
    void* p3 = arena.allocate(64, alignof(std::max_align_t));
    EXPECT_NE(p3, nullptr);
}

TEST(NumaPool, BasicAllocateDeallocate)
{
    struct Node { int x; int y; };

    constexpr std::size_t kCapacity = 128;
    hpc::core::numa_pool<Node> pool{kCapacity, -1};

    Node* nodes[kCapacity]{};

    for (std::size_t i = 0; i < kCapacity; ++i) {
        nodes[i] = pool.allocate();
        ASSERT_NE(nodes[i], nullptr);
        nodes[i]->x = static_cast<int>(i);
        nodes[i]->y = static_cast<int>(i * 2);
    }

    for (std::size_t i = 0; i < kCapacity; ++i) {
        EXPECT_EQ(nodes[i]->x, static_cast<int>(i));
        EXPECT_EQ(nodes[i]->y, static_cast<int>(i * 2));
    }

    for (std::size_t i = 0; i < kCapacity; ++i) {
        pool.deallocate(nodes[i]);
    }
}

} // namespace

