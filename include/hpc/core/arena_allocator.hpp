#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

#include <hpc/support/noncopyable.hpp>

namespace hpc::core {

// Simple bump-pointer arena with O(1) allocations and explicit reset.
class arena : private hpc::support::noncopyable {
public:
    explicit arena(std::size_t capacity_bytes);
    arena(void* buffer, std::size_t capacity_bytes);
    arena(arena&& other) noexcept;
    arena& operator=(arena&& other) noexcept;

    ~arena();

    void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept;

    void reset() noexcept;

    std::size_t capacity() const noexcept { return capacity_bytes_; }
    std::size_t used() const noexcept { return static_cast<std::size_t>(ptr_ - begin_); }

    void* data() noexcept { return begin_; }
    const void* data() const noexcept { return begin_; }

private:
    std::byte* begin_{};
    std::byte* end_{};
    std::byte* ptr_{};
    std::size_t capacity_bytes_{};
    bool owns_memory_{false};
};

// STL-compatible allocator backed by an arena.
template <class T>
class arena_allocator {
public:
    using value_type = T;

    explicit arena_allocator(arena& a) noexcept : arena_(&a) {}

    template <class U>
    arena_allocator(const arena_allocator<U>& other) noexcept : arena_(other.arena_) {}

    T* allocate(std::size_t n)
    {
        void* p = arena_->allocate(n * sizeof(T), alignof(T));
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }

    void deallocate(T*, std::size_t) noexcept {}

    template <class U>
    bool operator==(const arena_allocator<U>& rhs) const noexcept { return arena_ == rhs.arena_; }
    template <class U>
    bool operator!=(const arena_allocator<U>& rhs) const noexcept { return !(*this == rhs); }

    arena* arena_{};
};

} // namespace hpc::core

