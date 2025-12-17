#include <hpc/core/numa_arena.hpp>

#if defined(__linux__)
#include <numa.h>
#include <numaif.h>
#endif

namespace hpc::core {

namespace {

static bool numa_available() noexcept
{
#if defined(__linux__)
    return ::numa_available() != -1;
#else
    return false;
#endif
}

} // namespace

numa_arena::numa_arena(std::size_t size_bytes, int preferred_node) noexcept
    : arena_(size_bytes)
    , node_(preferred_node)
{
#if defined(__linux__)
    if (!numa_available() || preferred_node < 0) {
        node_ = -1;
        return;
    }

    // Best-effort: bind the arena backing memory to the preferred node.
    void* base = arena_.data();
    std::size_t len = arena_.capacity();
    if (!base || len == 0) {
        node_ = -1;
        return;
    }

    // Create a nodemask for the target node.
    struct bitmask* mask = ::numa_allocate_nodemask();
    if (!mask) {
        node_ = -1;
        return;
    }

    ::numa_bitmask_clearall(mask);
    if (preferred_node < static_cast<int>(mask->size)) {
        ::numa_bitmask_setbit(mask, preferred_node);
        ::mbind(base, len, MPOL_BIND, mask->maskp, mask->size, 0);
    }

    ::numa_free_nodemask(mask);
#else
    (void)size_bytes;
    (void)preferred_node;
#endif
}

} // namespace hpc::core

