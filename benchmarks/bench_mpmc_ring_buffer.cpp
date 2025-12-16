#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>
#include <vector>

#include <hpc/core/mpmc_ring_buffer.hpp>
#include <hpc/support/cpu_topology.hpp>

#include <queue>
#include <mutex>

namespace {

// Multi-producer / multi-consumer throughput benchmark comparing
// hpc::core::mpmc_ring_buffer with std::queue protected by std::mutex.

void BM_MPMCQueue_Throughput(benchmark::State& state)
{
    const std::size_t producers = static_cast<std::size_t>(state.range(0));
    const std::size_t consumers = static_cast<std::size_t>(state.range(1));
    const std::size_t total_ops = static_cast<std::size_t>(state.range(2));

    for (auto _ : state) {
        hpc::core::mpmc_ring_buffer<std::uint64_t> q(1u << 14);

        std::atomic<bool> start{false};
        std::atomic<std::size_t> produced{0};
        std::atomic<std::size_t> consumed{0};

        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;

        producer_threads.reserve(producers);
        consumer_threads.reserve(consumers);

        for (std::size_t p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&, p]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (;;) {
                    auto idx = produced.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= total_ops) break;
                    std::uint64_t v = static_cast<std::uint64_t>(idx);
                    while (!q.try_push(v)) {
                    }
                }
            });
        }

        for (std::size_t c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&, c]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (;;) {
                    if (consumed.load(std::memory_order_relaxed) >= total_ops) {
                        if (q.empty()) break;
                    }
                    std::uint64_t v;
                    if (q.try_pop(v)) {
                        benchmark::DoNotOptimize(v);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);

        for (auto& t : producer_threads) t.join();
        for (auto& t : consumer_threads) t.join();

        state.SetItemsProcessed(state.items_processed() + total_ops);
    }
}

void BM_StdQueue_Mutex_Throughput(benchmark::State& state)
{
    const std::size_t producers = static_cast<std::size_t>(state.range(0));
    const std::size_t consumers = static_cast<std::size_t>(state.range(1));
    const std::size_t total_ops = static_cast<std::size_t>(state.range(2));

    for (auto _ : state) {
        std::queue<std::uint64_t> q;
        std::mutex m;

        std::atomic<bool> start{false};
        std::atomic<std::size_t> produced{0};
        std::atomic<std::size_t> consumed{0};

        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;

        producer_threads.reserve(producers);
        consumer_threads.reserve(consumers);

        for (std::size_t p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&, p]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (;;) {
                    auto idx = produced.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= total_ops) break;
                    std::uint64_t v = static_cast<std::uint64_t>(idx);
                    {
                        std::lock_guard<std::mutex> lock(m);
                        q.push(v);
                    }
                }
            });
        }

        for (std::size_t c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&, c]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (;;) {
                    if (consumed.load(std::memory_order_relaxed) >= total_ops) {
                        std::lock_guard<std::mutex> lock(m);
                        if (q.empty()) break;
                    }
                    std::uint64_t v;
                    {
                        std::lock_guard<std::mutex> lock(m);
                        if (!q.empty()) {
                            v = q.front();
                            q.pop();
                        } else {
                            continue;
                        }
                    }
                    benchmark::DoNotOptimize(v);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        start.store(true, std::memory_order_release);

        for (auto& t : producer_threads) t.join();
        for (auto& t : consumer_threads) t.join();

        state.SetItemsProcessed(state.items_processed() + total_ops);
    }
}

} // namespace

BENCHMARK(BM_MPMCQueue_Throughput)
    ->Args({2, 2, 1 << 20})
    ->Unit(benchmark::kNanosecond);

BENCHMARK(BM_StdQueue_Mutex_Throughput)
    ->Args({2, 2, 1 << 20})
    ->Unit(benchmark::kNanosecond);
