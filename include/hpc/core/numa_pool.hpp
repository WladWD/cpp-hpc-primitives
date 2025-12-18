#pragma once

#include <cstddef>

#include <hpc/core/pool_allocator.hpp>
#include <hpc/core/numa_arena.hpp>

namespace hpc::core {

// NUMA-aware fixed-size pool. Internally uses a numa_arena as a hint for
// placement, but delegates block management to fixed_pool. On platforms
// without NUMA support this reduces to a regular fixed_pool.

template <class T>
class numa_pool {
public:
    explicit numa_pool(std::size_t capacity,
                       int preferred_node = -1)
        : arena_(capacity * sizeof(T), preferred_node)
        , pool_(sizeof(T), capacity)
    {}

    numa_pool(const numa_pool&) = delete;
    numa_pool& operator=(const numa_pool&) = delete;

    numa_pool(numa_pool&&) = delete;
    numa_pool& operator=(numa_pool&&) = delete;

    [[nodiscard]] T* allocate() noexcept { return static_cast<T*>(pool_.allocate()); }

    void deallocate(T* ptr) noexcept { pool_.deallocate(ptr); }

    std::size_t capacity() const noexcept { return pool_.capacity(); }

    int node() const noexcept { return arena_.node(); }

private:
    numa_arena arena_;   // currently only used to bias placement; could be
                         // used for backing storage in a future refinement.
    fixed_pool pool_;
};

} // namespace hpc::core

