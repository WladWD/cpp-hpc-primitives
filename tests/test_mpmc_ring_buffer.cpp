#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

#include <hpc/core/mpmc_ring_buffer.hpp>

namespace {

TEST(MPMCRingBufferBasic, SingleThreadPushPop)
{
    hpc::core::mpmc_ring_buffer<int> q(8);

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());

    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }

    int value = -1;
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.try_pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(q.empty());
}

TEST(MPMCRingBufferBasic, BatchInterfaces)
{
    hpc::core::mpmc_ring_buffer<int> q(16);

    int src[8];
    for (int i = 0; i < 8; ++i) src[i] = i;

    std::size_t pushed = q.try_push_batch(src, 8);
    EXPECT_EQ(pushed, 8u);

    int dst[8] = {};
    std::size_t popped = q.try_pop_batch(dst, 8);
    EXPECT_EQ(popped, 8u);

    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(dst[i], i);
    }
}

TEST(MPMCRingBufferConcurrency, MultiProducerMultiConsumer)
{
    constexpr std::size_t kCapacity = 1 << 10;
    constexpr std::size_t kProducers = 4;
    constexpr std::size_t kConsumers = 4;
    constexpr std::size_t kPerProducer = 1000;

    hpc::core::mpmc_ring_buffer<std::size_t> q(kCapacity);

    std::atomic<bool> start{false};
    std::atomic<std::size_t> produced{0};
    std::atomic<std::size_t> consumed{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::vector<std::vector<std::size_t>> consumer_data(kConsumers);

    const std::size_t total = kProducers * kPerProducer;

    for (std::size_t p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (;;) {
                std::size_t current = produced.load(std::memory_order_relaxed);
                while (current < total) {
                    if (produced.compare_exchange_weak(
                            current, current + 1,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                        auto idx = current;
                        while (!q.try_push(idx)) {
                        }
                        break; // move to next item
                    }
                    // CAS failed; current has been updated, loop and re-check bound
                }
                if (current >= total) {
                    break; // all items assigned
                }
            }
        });
    }

    for (std::size_t c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&, c]() {
            consumer_data[c].reserve(total / kConsumers + 1);
            while (!start.load(std::memory_order_acquire)) {
            }
            for (;;) {
                if (consumed.load(std::memory_order_relaxed) >= total) {
                    if (q.empty()) break;
                }
                std::size_t value;
                if (q.try_pop(value)) {
                    consumer_data[c].push_back(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(produced.load(), total);
    EXPECT_EQ(consumed.load(), total);

    std::unordered_set<std::size_t> seen;
    seen.reserve(total);
    for (const auto& vec : consumer_data) {
        for (auto v : vec) {
            auto [it, inserted] = seen.insert(v);
            EXPECT_TRUE(inserted);
        }
    }

    EXPECT_EQ(seen.size(), total);
}

} // namespace
