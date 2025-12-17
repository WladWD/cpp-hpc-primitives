#include <hpc/support/huge_pages.hpp>

#include <cstddef>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace hpc::support {

[[nodiscard]] huge_page_region huge_page_alloc(std::size_t size) noexcept
{
    huge_page_region region{};

#if defined(__linux__)
    // Best-effort attempt to use anonymous huge pages. If this fails, we
    // gracefully fall back to regular anonymous pages.
    long hpage_size = ::sysconf(_SC_HUGEPAGESIZE);
    if (hpage_size > 0) {
        std::size_t hp = static_cast<std::size_t>(hpage_size);
        std::size_t rounded = (size + hp - 1) & ~(hp - 1);

        void* ptr = ::mmap(nullptr, rounded,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                           -1, 0);
        if (ptr != MAP_FAILED) {
            region.ptr   = ptr;
            region.size  = rounded;
            region.align = hp;
            return region;
        }
    }

    // Fallback: regular anonymous mmap with default page size.
    long page_size = ::sysconf(_SC_PAGESIZE);
    std::size_t ps = page_size > 0 ? static_cast<std::size_t>(page_size) : 4096u;
    std::size_t rounded = (size + ps - 1) & ~(ps - 1);

    void* ptr = ::mmap(nullptr, rounded,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
    if (ptr == MAP_FAILED) {
        return region; // {nullptr,0}
    }

    region.ptr   = ptr;
    region.size  = rounded;
    region.align = ps;
    return region;

#elif defined(_WIN32)
    // On Windows, large pages require SeLockMemoryPrivilege and use
    // MEM_LARGE_PAGES. If the request fails
    // (insufficient privilege or configuration), we fall back to regular
    // VirtualAlloc pages.

    SIZE_T large_page_min = ::GetLargePageMinimum();
    if (large_page_min != 0) {
        SIZE_T rounded = static_cast<SIZE_T>((size + large_page_min - 1) & ~(large_page_min - 1));
        void* ptr = ::VirtualAlloc(nullptr, rounded,
                                   MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                   PAGE_READWRITE);
        if (ptr != nullptr) {
            region.ptr   = ptr;
            region.size  = rounded;
            region.align = static_cast<std::size_t>(large_page_min);
            return region;
        }
    }

    // Fallback: regular page-sized VirtualAlloc.
    SYSTEM_INFO info{};
    ::GetSystemInfo(&info);
    std::size_t ps = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096u;
    SIZE_T rounded = static_cast<SIZE_T>((size + ps - 1) & ~(ps - 1));

    void* ptr = ::VirtualAlloc(nullptr, rounded,
                               MEM_RESERVE | MEM_COMMIT,
                               PAGE_READWRITE);
    if (ptr == nullptr) {
        return region;
    }

    region.ptr   = ptr;
    region.size  = rounded;
    region.align = ps;
    return region;

#else
    (void)size;
    return region;
#endif
}

void huge_page_free(const huge_page_region& region) noexcept
{
#if defined(__linux__)
    if (region.ptr && region.size) {
        ::munmap(region.ptr, region.size);
    }
#elif defined(_WIN32)
    if (region.ptr) {
        ::VirtualFree(region.ptr, 0, MEM_RELEASE);
    }
#else
    (void)region;
#endif
}

} // namespace hpc::support

