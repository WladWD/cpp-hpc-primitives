#include <hpc/support/huge_pages.hpp>

#include <cstddef>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace hpc::support {

#if defined(__linux__)
namespace {
[[nodiscard]] std::size_t linux_huge_page_size_bytes() noexcept
{
    // _SC_HUGEPAGESIZE is not available on all libcs (e.g. it may be missing on
    // some glibc configurations / non-glibc libcs). Query /proc/meminfo instead.
    // Format example: "Hugepagesize:       2048 kB".
    std::size_t kb = 0;
    if (FILE* f = std::fopen("/proc/meminfo", "r")) {
        char line[256];
        while (std::fgets(line, sizeof(line), f) != nullptr) {
            // NOLINTNEXTLINE(cert-err34-c): best-effort parse
            if (std::sscanf(line, "Hugepagesize:%zu kB", &kb) == 1) {
                break;
            }
        }
        std::fclose(f);
    }

    if (kb == 0) {
        return 0;
    }

    return kb * 1024u;
}
} // namespace
#endif

[[nodiscard]] huge_page_region huge_page_alloc(std::size_t size) noexcept
{
    huge_page_region region{};

#if defined(__linux__)
    // Best-effort attempt to use explicit huge pages. If this fails, we
    // gracefully fall back to regular anonymous pages.
    const std::size_t hp = linux_huge_page_size_bytes();
    if (hp != 0) {
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

