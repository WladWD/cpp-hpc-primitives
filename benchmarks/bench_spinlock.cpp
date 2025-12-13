#include <benchmark/benchmark.h>

#include <hpc/core/ttas_spinlock.hpp>

#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

namespace {

constexpr std::size_t kNumThreads = 4;
constexpr std::size_t kIterationsPerThread = 1 << 14;

void BM_TTAS_Spinlock_Contention(benchmark::State& state)
{
    for (auto _ : state) {
        hpc::core::ttas_spinlock lock;
        std::atomic<std::uint64_t> counter{0};
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);

        for (std::size_t t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([&]() {
                for (std::size_t i = 0; i < kIterationsPerThread; ++i) {
                    lock.lock();
                    ++counter;
                    lock.unlock();
                }
            });
        }

        for (auto& th : threads) {
            th.join();
        }

        // Each iteration accounts for all increments performed by all threads.
        state.SetItemsProcessed(state.items_processed() + kNumThreads * kIterationsPerThread);
    }
}

void BM_StdMutex_Contention(benchmark::State& state)
{
    for (auto _ : state) {
        std::mutex m;
        std::uint64_t counter = 0;
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);

        for (std::size_t t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([&]() {
                for (std::size_t i = 0; i < kIterationsPerThread; ++i) {
                    std::lock_guard<std::mutex> g(m);
                    ++counter;
                }
            });
        }

        for (auto& th : threads) {
            th.join();
        }

        state.SetItemsProcessed(state.items_processed() + kNumThreads * kIterationsPerThread);
    }
}

} // namespace

BENCHMARK(BM_TTAS_Spinlock_Contention);
BENCHMARK(BM_StdMutex_Contention);

