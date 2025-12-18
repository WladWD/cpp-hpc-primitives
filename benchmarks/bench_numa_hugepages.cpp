#include <benchmark/benchmark.h>
#include <cstdlib>
#include <new>

#include <hpc/support/huge_pages.hpp>
#include <hpc/core/numa_arena.hpp>
#include <hpc/core/numa_pool.hpp>

namespace {

static void BM_Malloc_Free(benchmark::State& state)
{
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        void* p = std::malloc(size);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}

BENCHMARK(BM_Malloc_Free)->Arg(1 << 20)->Arg(1 << 24);

static void BM_New_Delete(benchmark::State& state)
{
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        char* p = nullptr;
        try {
            p = new char[size];
        } catch (...) {
            p = nullptr;
        }
        benchmark::DoNotOptimize(p);
        delete[] p;
    }
}

BENCHMARK(BM_New_Delete)->Arg(1 << 20)->Arg(1 << 24);

static void BM_HugePageAlloc_Free(benchmark::State& state)
{
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        auto region = hpc::support::huge_page_alloc(size);
        benchmark::DoNotOptimize(region.ptr);
        hpc::support::huge_page_free(region);
    }
}

BENCHMARK(BM_HugePageAlloc_Free)->Arg(1 << 20)->Arg(1 << 24);

static void BM_NumaArena_Allocate(benchmark::State& state)
{
    const std::size_t arena_size = static_cast<std::size_t>(state.range(0));
    hpc::core::numa_arena arena{arena_size, -1};

    for (auto _ : state) {
        state.PauseTiming();
        arena.reset();
        state.ResumeTiming();

        std::size_t allocated = 0;
        while (allocated + 64 <= arena.capacity()) {
            void* p = arena.allocate(64, alignof(std::max_align_t));
            if (!p) break;
            benchmark::DoNotOptimize(p);
            allocated += 64;
        }
    }
}

BENCHMARK(BM_NumaArena_Allocate)->Arg(1 << 16)->Arg(1 << 20);

static void BM_NumaPool_Allocate(benchmark::State& state)
{
    struct Node { int x; int y; };

    const std::size_t capacity = static_cast<std::size_t>(state.range(0));
    hpc::core::numa_pool<Node> pool{capacity, -1};

    for (auto _ : state) {
        // We don't reset the pool between iterations; each allocation is O(1)
        // and we only care about allocation cost here.
        for (std::size_t i = 0; i < capacity; ++i) {
            Node* n = pool.allocate();
            if (!n) break;
            benchmark::DoNotOptimize(n);
            pool.deallocate(n);
        }
    }
}

BENCHMARK(BM_NumaPool_Allocate)->Arg(128)->Arg(1024);

} // namespace
