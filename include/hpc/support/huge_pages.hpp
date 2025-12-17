#pragma once

#include <cstddef>

namespace hpc::support {

// Huge page allocation utilities.
//
// On Linux, this will attempt to allocate from anonymous huge pages using
// mmap + MAP_HUGETLB when available. On other platforms, or when the
// allocation fails, it transparently falls back to regular page-sized
// anonymous memory.
//
// This interface is deliberately minimal and does not attempt to manage
// fragmentation or reservations; callers should allocate a small number of
// large regions and sub-allocate from them.

struct huge_page_region {
    void*        ptr   = nullptr;
    std::size_t  size  = 0;     // bytes actually mapped
    std::size_t  align = 0;     // alignment of the mapping (for debugging)
};

// Request a region of at least `size` bytes, ideally backed by huge pages.
// The returned size may be rounded up to a huge-page multiple where
// applicable.
[[nodiscard]] huge_page_region huge_page_alloc(std::size_t size) noexcept;

// Release a region previously returned by huge_page_alloc(). Safe to call
// with {nullptr, 0} and will no-op in that case.
void huge_page_free(const huge_page_region& region) noexcept;

} // namespace hpc::support
