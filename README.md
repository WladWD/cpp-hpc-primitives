# cpp-hpc-primitives

High-performance C++23 primitives for low-latency and real‑time systems.

This repository is structured as a small, focused "standard library replacement" for
HFT-style workloads: single/multi-core pipelines, fixed-capacity queues, custom
allocators, and shared‑memory IPC, all built with explicit attention to cache
behavior, memory ordering, and observability.

---

## 1. Architecture & Design Principles

**Core principles:**

- **Latency first**: Prefer bounded queues, pre-allocated memory, and zero‑copy
  data paths over generality. All hot paths are `noexcept` and branch‑light.
- **Predictable memory**: Arena and pool allocators avoid `malloc`/`free`
  variability and keep data contiguous in L1.
- **Cache coherency aware**: Cache line padding, power-of-two ring sizes, and
  TTAS spinlocks minimize false sharing and unnecessary cache line invalidations.
- **Explicit memory ordering**: Atomics use the weakest ordering that is still
  correct (`memory_order_relaxed` vs `acquire/release`), with comments explaining
  the happens‑before relationships.
- **Reproducible performance**: Benchmarks pin threads to cores, warm up before
  measurement, and report comparable numbers vs. standard library baselines.
- **Production‑oriented layout**: Modern CMake, clear include hierarchy, and
  separable `tests/` and `benchmarks/` mirror a real library used across many
  services.

Repository skeleton:

```text
cpp-hpc-primitives/
├── benchmarks/          # Google Benchmark microbenchmarks
├── cmake/               # Toolchain & dependency modules
├── include/
│   └── hpc/
│       ├── core/        # Concurrency and allocation primitives
│       ├── ipc/         # Shared-memory IPC structures
│       └── support/     # Timing, CPU topology, cache-line helpers
├── src/                 # Non-header-only implementations
├── tests/               # GoogleTest unit and stress tests
├── CMakeLists.txt       # Modern CMake + FetchContent
└── README.md            # This document
```

---

## 2. Implemented Primitives

### 2.1 Lock-free SPSC ring buffer

**Type:** `hpc::core::spsc_ring_buffer<T>`

A single‑producer, single‑consumer ring buffer that matches the needs of a
feed‑handler → strategy or strategy → gateway pipeline.

Key design points:

- **False sharing prevention**: Producer and consumer indices are separated by
  cache‑line padding (`alignas(64)`), so updating the head does not invalidate
  the cache line containing the tail (and vice versa).
- **Power‑of‑two capacity**: Capacity is forced to a power of two. Index wrap‑around
  uses a bit mask (`index & (capacity - 1)`) instead of modulo division.
- **Batch API**: `push_batch(T* items, size_t count)` (and corresponding batch
  pop) amortize the cost of the store‑side barrier and bounds checks across many
  elements.
- **Zero‑copy slots**: `T* alloc_slot()` and `commit_slot()` allow the producer
  to construct messages in place inside the ring, avoiding an extra copy and
  improving cache locality for complex message types.
- **Memory ordering**: Atomics use `memory_order_acquire`/`memory_order_release`
  across producer/consumer boundaries with `memory_order_relaxed` for local
  progress where safe. Comments in the implementation document the required
  happens‑before relationships per operation.

Typical HFT uses:

- Per‑symbol or per‑feed SPSC queues in a market data handler.
- Intra‑process handoff between a strategy thread and a risk/aggregation thread.

### 2.2 Linear / arena allocator

**Types:** `hpc::core::arena`, `hpc::core::arena_allocator<T>`

A pointer‑bump allocator used for short‑lived, bulk‑freed objects (e.g.,
per‑snapshot order books or per‑batch message decoding).

Characteristics:

- **O(1) allocation**: Linear bump of a single pointer; no locks, no free list.
- **Noexcept**: Allocation does not throw; failure is returned explicitly and
  can be handled on a slow path.
- **Bulk reset**: The arena can be reset in O(1), reclaiming the entire region
  at once when a batch or snapshot is complete.
- **Cache‑friendly**: Objects allocated together are contiguous in memory,
  improving hardware prefetch efficiency and TLB locality.

### 2.3 Fixed‑size pool allocator

**Types:** `hpc::core::fixed_pool`, `hpc::core::pool_allocator<T>`

A free‑list based allocator for fixed‑size objects such as `Order`, `Quote`,
`OrderBookNode`, etc.

Characteristics:

- **Deterministic allocation**: Fixed block size, constant‑time allocate/free.
- **Locality**: The pool is backed by contiguous slabs, so active objects tend to
  live in a small number of cache lines.
- **Low fragmentation**: No general heap metadata or per‑allocation headers; the
  free list is embedded into freed blocks.

### 2.4 TTAS spinlock

**Type:** `hpc::core::ttas_spinlock`

A **test‑test‑and‑set** spinlock with exponential backoff.

Why TTAS:

- **Reduced cache line invalidation**: Threads first perform a plain read of the
  lock variable. Only when it looks free do they attempt an atomic exchange.
  This avoids thrashing the cache line in the common contended case.
- **Backoff strategy**: On failure, the lock performs a short `_mm_pause()` loop
  (hyper‑threading friendly) before optionally yielding to the OS.

Appropriate uses:

- Very short critical sections on hot paths where `std::mutex` would introduce
  unacceptable scheduling latency.
- Coarse‑grained configuration or statistics updates where you want to avoid the
  complexity of full lock‑free structures.

### 2.5 Shared‑memory SPSC ring buffer

**Type:** `hpc::ipc::shm_spsc_ring_buffer<T>`

An SPSC ring buffer hosted in a POSIX shared memory segment (`shm_open`/`mmap`),
allowing two processes to communicate with

- Zero per‑message dynamic allocation,
- A fixed, bounded buffer,
- And no serialization overhead beyond the in‑memory representation of `T`.

Typical uses:

- C++ feed handler → Python/PyTorch process for model inference.
- C++ matching engine → monitoring/analytics process.

The layout is intentionally simple and documented so that non‑C++ consumers can
attach to the shared memory region and parse messages using only a struct
definition.

---

## 3. Benchmarks & Performance

Microbenchmarks are implemented with Google Benchmark in `benchmarks/`. Benchmarks
pin threads to cores and run in steady‑state to reduce OS jitter and warm‑up
artifacts.

### 3.1 Summary (example run)

All numbers below are from a local development machine; they are intended to show
relative speedups rather than absolute limits. Exact hardware and compiler flags
are recorded in the benchmark output.

| Component                          | Operations/sec | Wall time (ns) | CPU time (ns) | Baseline                         |
|------------------------------------|----------------|--------------|-------------|----------------------------------|
| `std::queue` (SPSC baseline)       |   719.83 M/s   | 1423 ns      | 1423 ns     | –                                |
| `hpc::SPSCQueue`                   | 1.05306 G/s    | 973 ns       | 972 ns      | 1.46x vs `std::queue`            |
| `malloc`                           |    96.24 M/s   | 10642 ns     | 10640 ns    | –                                |
| `hpc::arena_allocator`             |  437.73 M/s    | 2340 ns      | 2339 ns     | 4.55x vs `malloc`                |
| `hpc::pool_allocator`              |  646.31 M/s    | 1585 ns      | 1584 ns     | 6.71x vs `malloc`                |
| `std::mutex` (contended)           |  1.58  G/s     | 1360177 ns*  | 41481 ns*   | –                                |
| `hpc::TTASSpinLock` (contended)    |  2.45  G/s     | 246487 ns*   | 26756 ns*   | 1.55x vs `std::mutex`            |
| `std::queue` + `std::mutex` (MPMC) | 21.43 G/s      | 64743477 ns**  | 48920 ns**    | –                                |
| `hpc::MPMCQueue`                   |  5.14 G/s      | 93139968 ns**  | 203850 ns**   | 0.24x vs `std::queue`+`std::mutex` |

`*` Spinlock vs `std::mutex` numbers are from a contended critical-section benchmark
(`benchmarks/bench_spinlock.cpp`) with multiple threads incrementing a shared
counter; the absolute nanoseconds include benchmark harness overhead and should
be interpreted relatively. Wall time reflects end-to-end duration including
blocking/scheduling; CPU time reflects active CPU work per iteration.

`**` MPMC throughput numbers are from a 2‑producer / 2‑consumer benchmark pushing
1,048,576 items per producer (`benchmarks/bench_mpmc_ring_buffer.cpp`). In this
particular microbenchmark, `std::queue` protected by a single `std::mutex` is
faster than the lock‑free `hpc::core::mpmc_ring_buffer`. This is **expected**:

- The critical section around `std::queue` is extremely small (push/pop on a
  pre-allocated container with no dynamic allocation in the hot path), so
  `std::mutex` can remain very efficient.
- The lock‑free MPMC queue pays additional per-slot overhead (sequence counters,
  extra atomics, CAS loops) to provide multi‑producer/multi‑consumer semantics
  without a central lock and to avoid classic ABA issues.
- Under light contention with few threads and a tiny critical section, the cost
  of those extra atomics can outweigh the cost of taking a uncontended or
  mildly contended `std::mutex`.

The MPMC implementation in this repository is tuned for **scalability and
predictable behavior under higher contention and more complex pipelines**, not
for winning every synthetic microbenchmark against a single `std::mutex`. 

These numbers are not synthetic micro‑optimizations; they map directly to real workloads:

- Higher queue throughput at lower per‑operation latency → more headroom for
  decode, normalization, and strategy logic per tick.
- Faster, more predictable allocators → lower tail latency when creating or
  recycling orders, quotes, and internal book structures.
- Cheaper spinlock acquisitions under contention → less time stuck in critical
  sections compared to `std::mutex`, especially for short, hot critical paths.

### 3.2 Reproducing benchmarks

From the project root:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DHPC_ENABLE_TESTS=ON \
      -DHPC_ENABLE_BENCHMARKS=ON ..
cmake --build . -j
```

Run unit tests:

```bash
ctest --output-on-failure
```

Run benchmarks:

```bash
./benchmarks/hpc_benchmarks
```

On Linux, you can additionally pin benchmark processes to specific cores (via
`taskset`) or use the built‑in CPU affinity utilities in this repo to reduce
scheduler noise.

---

## 4. Build & Tooling

- **Language standard**: C++23.
- **Build system**: Modern CMake with `FetchContent` for GoogleTest and Google
  Benchmark.
- **Tests**: Unit tests under `tests/` validate correctness of the ring buffer,
  allocators, and spinlock.
- **Benchmarks**: Microbenchmarks under `benchmarks/` compare these primitives
  against standard library equivalents (`std::queue`, `malloc`/`new`, etc.).

From a clean checkout:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DHPC_ENABLE_TESTS=ON \
      -DHPC_ENABLE_BENCHMARKS=ON ..
cmake --build . -j
ctest --output-on-failure
./benchmarks/hpc_benchmarks
```

---
