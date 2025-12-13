#include <benchmark/benchmark.h>

#include <hpc/core/arena_allocator.hpp>
#include <hpc/core/pool_allocator.hpp>

#include <cstdlib>

namespace {

struct alignas(64) payload {
    std::uint64_t data[8];
};

void BM_Malloc(benchmark::State& state)
{
    for (auto _ : state) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
            auto* p = static_cast<payload*>(std::malloc(sizeof(payload)));
            benchmark::DoNotOptimize(p);
            std::free(p);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_ArenaAlloc(benchmark::State& state)
{
    hpc::core::arena arena(1 << 24);

    for (auto _ : state) {
        arena.reset();
        for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
            auto* p = static_cast<payload*>(arena.allocate(sizeof(payload), alignof(payload)));
            benchmark::DoNotOptimize(p);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_PoolAlloc(benchmark::State& state)
{
    hpc::core::fixed_pool pool(sizeof(payload), 1 << 16);

    for (auto _ : state) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
            auto* p = static_cast<payload*>(pool.allocate());
            if (!p) break;
            benchmark::DoNotOptimize(p);
            pool.deallocate(p);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_Malloc)->Arg(1 << 10);
BENCHMARK(BM_ArenaAlloc)->Arg(1 << 10);
BENCHMARK(BM_PoolAlloc)->Arg(1 << 10);

