#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include <hpc/support/noncopyable.hpp>

namespace hpc::core {

// Fixed-size object pool with a free-list stored in freed blocks.
class fixed_pool : private hpc::support::noncopyable {
public:
    fixed_pool(std::size_t element_size, std::size_t element_count);
    ~fixed_pool();

    void* allocate();
    void deallocate(void* p) noexcept;

    std::size_t capacity() const noexcept { return element_count_; }

private:
    struct node { node* next; };

    std::size_t element_size_{};
    std::size_t element_count_{};
    std::byte* storage_{};
    node* free_list_{};
};

// STL-style allocator on top of fixed_pool.
template <class T>
class pool_allocator {
public:
    using value_type = T;

    explicit pool_allocator(fixed_pool& pool) noexcept : pool_(&pool) {}

    template <class U>
    pool_allocator(const pool_allocator<U>& other) noexcept : pool_(other.pool_) {}

    T* allocate(std::size_t n)
    {
        if (n != 1) throw std::bad_alloc();
        void* p = pool_->allocate();
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept { pool_->deallocate(p); }

    template <class U>
    bool operator==(const pool_allocator<U>& rhs) const noexcept { return pool_ == rhs.pool_; }
    template <class U>
    bool operator!=(const pool_allocator<U>& rhs) const noexcept { return !(*this == rhs); }

    fixed_pool* pool_{};
};

} // namespace hpc::core

