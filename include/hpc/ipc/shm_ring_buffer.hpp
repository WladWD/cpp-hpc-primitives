#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include <hpc/support/noncopyable.hpp>

namespace hpc::ipc {

// Shared-memory backed SPSC ring buffer for IPC between processes.
// Layout:
//   [ header | slots[capacity] ]
// Where header stores indices and capacity, and slots are plain T objects.

struct shm_ring_config {
    std::string name;      // POSIX shared memory name, e.g. "/hpc_ring"
    std::size_t capacity;  // number of elements (slots)
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

#pragma pack(push, 8)
struct shm_spsc_header {
    std::uint64_t capacity; // number of slots
    std::uint64_t head;     // consumer index
    std::uint64_t tail;     // producer index
};
#pragma pack(pop)


template <class T>
class shm_spsc_ring_buffer {
public:
    explicit shm_spsc_ring_buffer(const shm_ring_config& cfg);

    bool try_push(const T& value);
    bool try_pop(T& out);

    std::size_t capacity() const noexcept { return header_->capacity; }

private:
    shm_region region_;
    shm_spsc_header* header_{};
    T* slots_{};
};

template <class T>
shm_spsc_ring_buffer<T>::shm_spsc_ring_buffer(const shm_ring_config& cfg)
    : region_{[&cfg] {
          const std::size_t bytes = sizeof(shm_spsc_header) + cfg.capacity * sizeof(T);
          shm_ring_config tmp = cfg;
          tmp.capacity = bytes;
          return shm_region{tmp};
      }()}
{
    auto* base = static_cast<std::byte*>(region_.address());
    header_ = reinterpret_cast<shm_spsc_header*>(base);
    header_->capacity = cfg.capacity;
    header_->head = 0;
    header_->tail = 0;

    slots_ = reinterpret_cast<T*>(base + sizeof(shm_spsc_header));
}

template <class T>
bool shm_spsc_ring_buffer<T>::try_push(const T& value)
{
    const auto cap = header_->capacity;
    auto tail = header_->tail;
    auto head = header_->head;
    if (((tail + 1) % cap) == head) {
        return false; // full
    }
    slots_[tail] = value;
    header_->tail = (tail + 1) % cap;
    return true;
}

template <class T>
bool shm_spsc_ring_buffer<T>::try_pop(T& out)
{
    const auto cap = header_->capacity;
    auto head = header_->head;
    auto tail = header_->tail;
    if (head == tail) {
        return false; // empty
    }
    out = slots_[head];
    header_->head = (head + 1) % cap;
    return true;
}

} // namespace hpc::ipc

