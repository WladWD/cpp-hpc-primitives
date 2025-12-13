#include <hpc/core/arena_allocator.hpp>

#include <algorithm>
#include <cassert>

namespace hpc::core {

namespace {

inline std::byte* align_ptr(std::byte* ptr, std::size_t alignment) noexcept
{
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    auto aligned = (addr + (alignment - 1)) & ~(alignment - 1);
    return reinterpret_cast<std::byte*>(aligned);
}

} // namespace

arena::arena(std::size_t capacity_bytes)
{
    begin_ = static_cast<std::byte*>(::operator new(capacity_bytes, std::nothrow));
    end_ = begin_ ? begin_ + capacity_bytes : nullptr;
    ptr_ = begin_;
    capacity_bytes_ = capacity_bytes;
    owns_memory_ = true;
}

arena::arena(void* buffer, std::size_t capacity_bytes)
{
    begin_ = static_cast<std::byte*>(buffer);
    end_ = begin_ + capacity_bytes;
    ptr_ = begin_;
    capacity_bytes_ = capacity_bytes;
    owns_memory_ = false;
}

arena::arena(arena&& other) noexcept
{
    begin_ = other.begin_;
    end_ = other.end_;
    ptr_ = other.ptr_;
    capacity_bytes_ = other.capacity_bytes_;
    owns_memory_ = other.owns_memory_;

    other.begin_ = other.end_ = other.ptr_ = nullptr;
    other.capacity_bytes_ = 0;
    other.owns_memory_ = false;
}

arena& arena::operator=(arena&& other) noexcept
{
    if (this != &other) {
        if (owns_memory_ && begin_) {
            ::operator delete(begin_);
        }
        begin_ = other.begin_;
        end_ = other.end_;
        ptr_ = other.ptr_;
        capacity_bytes_ = other.capacity_bytes_;
        owns_memory_ = other.owns_memory_;

        other.begin_ = other.end_ = other.ptr_ = nullptr;
        other.capacity_bytes_ = 0;
        other.owns_memory_ = false;
    }
    return *this;
}

arena::~arena()
{
    if (owns_memory_ && begin_) {
        ::operator delete(begin_);
    }
}

void* arena::allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    if (!begin_) return nullptr;
    auto aligned = align_ptr(ptr_, alignment);
    if (aligned + bytes > end_) {
        return nullptr;
    }
    ptr_ = aligned + bytes;
    return aligned;
}

void arena::reset() noexcept
{
    ptr_ = begin_;
}

} // namespace hpc::core

