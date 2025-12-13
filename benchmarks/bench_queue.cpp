#include <benchmark/benchmark.h>

#include <hpc/core/ring_buffer.hpp>
#include <hpc/support/cpu_topology.hpp>

#include <queue>
#include <thread>

namespace {

void BM_SPSCQueue_Throughput(benchmark::State& state)
{
    constexpr std::size_t capacity = 1 << 16;
    hpc::core::spsc_ring_buffer<std::uint64_t> q(capacity);

    for (auto _ : state) {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
            while (!q.try_push(value)) {
            }
            while (!q.try_pop(value)) {
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_StdQueue_Throughput(benchmark::State& state)
{
    std::queue<std::uint64_t> q;

    for (auto _ : state) {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
            q.push(value);
            value = q.front();
            q.pop();
        }
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_SPSCQueue_Throughput)->Arg(1 << 10);
BENCHMARK(BM_StdQueue_Throughput)->Arg(1 << 10);

