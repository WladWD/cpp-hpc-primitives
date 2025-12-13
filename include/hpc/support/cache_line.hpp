#pragma once

#include <cstddef>

namespace hpc::support {

// Conservative default cache line size; can be specialized per-platform.
inline constexpr std::size_t cache_line_size = 64;

} // namespace hpc::support

