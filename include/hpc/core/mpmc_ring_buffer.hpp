#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

#include <hpc/support/cache_line.hpp>

namespace hpc::core {

// Bounded multi-producer multi-consumer ring buffer (Vyukov-style).
//
// Design notes:
//  - Each slot carries a monotonically increasing sequence counter. Producers
//    and consumers use this to determine whether a slot is empty or full
//    without additional locks, which also avoids classic ABA issues.
//  - Capacity is rounded up to the next power-of-two, allowing index
//    wrap-around with a bitwise AND instead of modulo.
//  - Head and tail indices are cache-line padded to avoid false sharing
//    between producers and consumers.
//  - Publication of elements uses release semantics; readers use acquire
//    semantics. Most index arithmetic is relaxed.
//  - Size/empty/full queries are intentionally approximate under concurrency
//    and are meant for observability, not correctness.

template <class T>
class mpmc_ring_buffer {
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible for lock-free teardown");

public:
    explicit mpmc_ring_buffer(std::size_t capacity)
        : capacity_(round_up_to_power_of_two(capacity))
        , mask_(capacity_ - 1)
        , cells_(static_cast<cell*>(::operator new[](capacity_ * sizeof(cell))))
    {
        // Initialize per-slot sequence numbers so that slot i is initially
        // observed as empty by producers (seq == i).
        for (std::size_t i = 0; i < capacity_; ++i) {
            ::new (static_cast<void*>(cells_ + i)) cell{i};
        }
    }

    ~mpmc_ring_buffer()
    {
        // As with the SPSC queue, we assume that all producers and consumers
        // have drained the queue before destruction to avoid walking remaining
        // elements and paying for potentially expensive destructors.
        for (std::size_t i = 0; i < capacity_; ++i) {
            cells_[i].~cell();
        }
        ::operator delete[](cells_);
    }

    mpmc_ring_buffer(const mpmc_ring_buffer&) = delete;
    mpmc_ring_buffer& operator=(const mpmc_ring_buffer&) = delete;

    mpmc_ring_buffer(mpmc_ring_buffer&&) = delete;
    mpmc_ring_buffer& operator=(mpmc_ring_buffer&&) = delete;

    std::size_t capacity() const noexcept { return capacity_; }

    bool empty() const noexcept
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_relaxed);
        return head == tail;
    }

    // Approximate: can return false negatives under contention.
    bool full() const noexcept
    {
        auto tail = tail_.value.load(std::memory_order_relaxed);
        const cell& c = cells_[tail & mask_];
        auto seq = c.sequence.load(std::memory_order_acquire);
        return seq < tail;
    }

    std::size_t approximate_size() const noexcept
    {
        auto head = head_.value.load(std::memory_order_relaxed);
        auto tail = tail_.value.load(std::memory_order_relaxed);
        return tail - head;
    }

    template <class... Args>
    bool try_emplace(Args&&... args)
    {
        index_type tail = tail_.value.load(std::memory_order_relaxed);
        for (;;) {
            cell& c = cells_[tail & mask_];
            std::size_t seq = c.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail);

            if (diff == 0) {
                // Slot is free; try to claim this tail index.
                if (tail_.value.compare_exchange_weak(
                        tail, tail + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    // We own this slot.
                    ::new (static_cast<void*>(std::addressof(c.storage))) T(std::forward<Args>(args)...);
                    // Publish element: sequence moves to tail + 1.
                    c.sequence.store(tail + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed: another producer moved tail; reload and retry.
            } else if (diff < 0) {
                // This slot sequence is behind the tail; the queue is full.
                return false;
            }

            // Another producer beat us to this slot; reload tail and try again.
            tail = tail_.value.load(std::memory_order_relaxed);
        }
    }

    bool try_push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        return try_emplace(value);
    }

    bool try_push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return try_emplace(std::move(value));
    }

    bool try_pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        index_type head = head_.value.load(std::memory_order_relaxed);
        for (;;) {
            cell& c = cells_[head & mask_];
            std::size_t seq = c.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head + 1);

            if (diff == 0) {
                // Element is available; try to claim this head index.
                if (head_.value.compare_exchange_weak(
                        head, head + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    // We own this slot.
                    T* value_ptr = reinterpret_cast<T*>(std::addressof(c.storage));
                    out = std::move(*value_ptr);
                    value_ptr->~T();
                    // Mark slot as empty for the next cycle: advance sequence
                    // by capacity_.
                    c.sequence.store(head + capacity_, std::memory_order_release);
                    return true;
                }
                // CAS failed, someone else moved head; reload and retry.
            } else if (diff < 0) {
                // Sequence is behind the expected head+1; queue is empty.
                return false;
            }

            head = head_.value.load(std::memory_order_relaxed);
        }
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

private:
    using index_type = std::size_t;

    struct cell {
        std::atomic<index_type> sequence;
        alignas(hpc::support::cache_line_size) std::aligned_storage_t<sizeof(T), alignof(T)> storage;

        explicit cell(index_type seq) noexcept : sequence(seq) {}
    };

    struct alignas(hpc::support::cache_line_size) padded_index {
        std::atomic<index_type> value{0};
    };

    static std::size_t round_up_to_power_of_two(std::size_t n) noexcept
    {
        if (n < 2) return 2;
        --n;
        for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1) {
            n |= n >> i;
        }
        return n + 1;
    }

    std::size_t capacity_{}; // usable capacity (power-of-two)
    std::size_t mask_{};

    padded_index head_{}; // consumer index
    padded_index tail_{}; // producer index

    cell* cells_{};
};

} // namespace hpc::core

