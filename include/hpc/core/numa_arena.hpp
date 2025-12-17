#pragma once

#include <cstddef>

#include <hpc/core/arena_allocator.hpp>

namespace hpc::core {

// NUMA-aware arena: a thin wrapper that allows binding the underlying
// arena storage to a specific NUMA node where the platform supports it.
//
// On platforms without NUMA APIs available (including macOS), this class
// gracefully degrades to a regular arena.

class numa_arena {
public:
    explicit numa_arena(std::size_t size_bytes,
                        int preferred_node = -1) noexcept;

    numa_arena(const numa_arena&) = delete;
    numa_arena& operator=(const numa_arena&) = delete;

    numa_arena(numa_arena&&) = delete;
    numa_arena& operator=(numa_arena&&) = delete;

    ~numa_arena() = default;

    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) noexcept
    {
        return arena_.allocate(size, align);
    }

    void reset() noexcept { arena_.reset(); }

    std::size_t capacity() const noexcept { return arena_.capacity(); }

    int node() const noexcept { return node_; }

    hpc::core::arena& underlying() noexcept { return arena_; }
    const hpc::core::arena& underlying() const noexcept { return arena_; }

private:
    hpc::core::arena arena_;
    int node_ = -1; // -1 == no specific NUMA binding
};

} // namespace hpc::core

