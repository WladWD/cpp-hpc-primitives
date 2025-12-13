# cpp-hpc-primitives

Production-grade C++23 library of lock-free data structures and memory primitives designed for sub-microsecond latency workloads. Features zero-allocation architecture, cache-aware memory layout, and shared-memory IPC.

## Components

- `hpc::core::spsc_ring_buffer<T>` — single-producer/single-consumer queue with cache-line padding, power-of-two capacity, batch APIs, and zero-copy slots.
- `hpc::core::arena` / `arena_allocator<T>` — bump-pointer allocator with O(1) allocations and explicit reset.
- `hpc::core::fixed_pool` / `pool_allocator<T>` — fixed-size object pool with free-list.
- `hpc::core::ttas_spinlock` — TTAS spinlock with backoff using `_mm_pause`.
- `hpc::ipc::shm_spsc_ring_buffer<T>` — shared-memory IPC ring buffer built on top of the in-process SPSC queue.

## Benchmarks 

| Component  | Operations/Sec | CPU time (ns) |
|------------|----------------|---------------|
| std::queue | 722.506M/s     | 1417ns        |
| malloc     | 100.324M/s     | 10207ns       |

| Component            | Operations/Sec | CPU time (ns) | Speedup vs Std      |
|----------------------|----------------|---------------|---------------------|
| hpc::SPSCQueue       | 1.05274G/s           | 973ns         | 1.62x vs std::queue |
| hpc::arena_allocator | 441.675M/s     | 2318ns        | 4.4x vs malloc      |
| hpc::pool_allocator  | 667.29M/s      | 1535ns        | 6.64x vs malloc |

## Building

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DHPC_ENABLE_TESTS=ON -DHPC_ENABLE_BENCHMARKS=ON ..
cmake --build . -j
ctest --output-on-failure
```

Run benchmarks:

```bash
./benchmarks/hpc_benchmarks
```
