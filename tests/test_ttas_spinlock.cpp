#include <gtest/gtest.h>

#include <hpc/core/ttas_spinlock.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST(TtasSpinlock, ContendedIncrement)
{
    hpc::core::ttas_spinlock lock;
    std::atomic<int> counter{0};

    constexpr int kThreads = 4;
    constexpr int kIters = 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                std::scoped_lock guard(lock);
                ++counter;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(counter.load(), kThreads * kIters);
}

