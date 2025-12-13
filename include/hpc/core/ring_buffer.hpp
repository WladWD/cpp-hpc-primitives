#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

#include <hpc/support/cache_line.hpp>

namespace hpc::core {

// Single-producer single-consumer ring buffer.
//
// Design notes:
//  - Indices are placed on separate cache lines to avoid false sharing.
//  - Capacity is rounded up to the next power-of-two so index wrap-around
//    uses a cheap bitwise AND instead of modulo.
//  - Producer publishes elements with release semantics; consumer observes
//    them with acquire semantics. Other loads can be relaxed.
//  - Provides batch APIs and zero-copy slot access to amortize fences and
//    avoid extra copies in the hot path.

template <class T>
class spsc_ring_buffer {
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible for lock-free teardown");

public:
    explicit spsc_ring_buffer(std::size_t capacity)
        : storage_capacity_(round_up_to_power_of_two(capacity + 1))
        , mask_(storage_capacity_ - 1)
        , storage_(static_cast<std::byte*>(::operator new[](storage_capacity_ * sizeof(T))))
    {
    }

    ~spsc_ring_buffer()
    {
        // In SPSC usage, producer and consumer are expected to have drained the
        // queue before destruction; we do not walk remaining elements.
        ::operator delete[](storage_);
    }

    spsc_ring_buffer(const spsc_ring_buffer&) = delete;
    spsc_ring_buffer& operator=(const spsc_ring_buffer&) = delete;

    bool try_push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        auto tail = tail_.value.load(std::memory_order_relaxed);
        auto head = head_.value.load(std::memory_order_acquire);
        if (distance(tail, head) == capacity()) {
            return false; // full
        }
        T* slot = element_at(tail);
        ::new (static_cast<void*>(slot)) T(value);
        tail_.value.store(next(tail), std::memory_order_release);
        return true;
    }

    bool try_push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        auto tail = tail_.value.load(std::memory_order_relaxed);
        auto head = head_.value.load(std::memory_order_acquire);
        if (distance(tail, head) == capacity()) {
            return false;
        }
        T* slot = element_at(tail);
        ::new (static_cast<void*>(slot)) T(std::move(value));
        tail_.value.store(next(tail), std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_acquire);
        if (head == tail) {
            return false; // empty
        }
        T* slot = element_at(head);
        out = std::move(*slot);
        slot->~T();
        head_.value.store(next(head), std::memory_order_release);
        return true;
    }

    std::size_t try_push_batch(const T* src, std::size_t count)
    {
        std::size_t pushed = 0;
        for (; pushed < count; ++pushed) {
            if (!try_push(src[pushed])) break;
        }
        return pushed;
    }

    std::size_t try_pop_batch(T* dst, std::size_t count)
    {
        std::size_t popped = 0;
        for (; popped < count; ++popped) {
            if (!try_pop(dst[popped])) break;
        }
        return popped;
    }

    T* try_acquire_producer_slot()
    {
        auto tail = tail_.value.load(std::memory_order_relaxed);
        auto head = head_.value.load(std::memory_order_acquire);
        if (distance(tail, head) == capacity()) {
            return nullptr;
        }
        return element_at(tail);
    }

    void commit_producer_slot()
    {
        auto tail = tail_.value.load(std::memory_order_relaxed);
        tail_.value.store(next(tail), std::memory_order_release);
    }

    T* try_acquire_consumer_slot()
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_acquire);
        if (head == tail) {
            return nullptr;
        }
        return element_at(head);
    }

    void release_consumer_slot()
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        head_.value.store(next(head), std::memory_order_release);
    }

    bool empty() const noexcept
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_relaxed);
        return head == tail;
    }

    bool full() const noexcept
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_relaxed);
        return distance(tail, head) == capacity();
    }

    std::size_t capacity() const noexcept { return storage_capacity_ - 1; }

private:
    static std::size_t round_up_to_power_of_two(std::size_t n) noexcept
    {
        if (n < 2) return 2;
        --n;
        for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1) {
            n |= n >> i;
        }
        return n + 1;
    }

    using index_type = std::size_t;

    struct alignas(hpc::support::cache_line_size) padded_index {
        std::atomic<index_type> value{0};
    };

    std::size_t storage_capacity_{}; // underlying ring size; usable capacity is storage_capacity_-1
    std::size_t mask_{};

    padded_index head_{}; // consumer-owned index (pop side)
    padded_index tail_{}; // producer-owned index (push side)

    std::byte* storage_{};

    index_type next(index_type idx) const noexcept { return (idx + 1) & mask_; }

    static std::size_t distance(index_type tail, index_type head) noexcept
    {
        return (tail - head) & static_cast<index_type>(-1);
    }

    T* element_at(index_type idx) const noexcept
    {
        auto pos = (idx & mask_) * sizeof(T);
        return reinterpret_cast<T*>(storage_ + pos);
    }
};

} // namespace hpc::core

