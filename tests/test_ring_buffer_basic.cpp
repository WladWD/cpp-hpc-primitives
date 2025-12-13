#include <gtest/gtest.h>

#include <hpc/core/ring_buffer.hpp>

TEST(SpscRingBuffer, BasicPushPop)
{
    // Underlying implementation reserves one slot to distinguish full vs empty.
    // Requesting capacity 8 yields a usable capacity of at least 7.
    hpc::core::spsc_ring_buffer<int> q(8);

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());

    int pushed = 0;
    for (; pushed < 8; ++pushed) {
        if (!q.try_push(pushed)) {
            break;
        }
    }

    // At least one element must have been enqueued.
    EXPECT_GT(pushed, 0);

    int value = 0;
    for (int i = 0; i < pushed; ++i) {
        EXPECT_TRUE(q.try_pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(q.empty());
}
