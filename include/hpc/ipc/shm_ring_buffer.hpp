#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include <hpc/core/ring_buffer.hpp>
#include <hpc/support/noncopyable.hpp>

namespace hpc::ipc {

// Shared-memory backed SPSC ring buffer for IPC between processes.
// This is a thin wrapper around the in-process SPSC ring, with the ring
// state placed inside a POSIX shared memory segment created via shm_open
// and mmap.

struct shm_ring_config {
    std::string name;      // POSIX shared memory name, e.g. "/hpc_ring"
    std::size_t capacity;  // number of elements (rounded to power-of-two)
    bool create;           // true for creator, false for attacher
};

class shm_region : private hpc::support::noncopyable {
public:
    shm_region(const shm_ring_config& cfg);
    ~shm_region();

    void* address() noexcept { return addr_; }
    const void* address() const noexcept { return addr_; }
    std::size_t size() const noexcept { return size_; }

private:
    int fd_{-1};
    void* addr_{nullptr};
    std::size_t size_{};
    std::string name_;
    bool owner_{false};
};

// Layout of the shared memory region:
// [ control block | element storage ... ]
// Where control block embeds the ring indices and configuration.

template <class T>
class shm_spsc_ring_buffer {
public:
    explicit shm_spsc_ring_buffer(const shm_ring_config& cfg);

    bool try_push(const T& value) { return ring_->try_push(value); }
    bool try_push(T&& value) { return ring_->try_push(std::move(value)); }

    bool try_pop(T& out) { return ring_->try_pop(out); }

private:
    struct control_block {
        std::size_t capacity;
        // ring_buffer<T> follows in-place in shared memory.
    };

    shm_region region_;
    hpc::core::spsc_ring_buffer<T>* ring_{};
};

} // namespace hpc::ipc

