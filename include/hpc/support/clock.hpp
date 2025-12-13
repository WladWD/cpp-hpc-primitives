#pragma once

#include <chrono>
#include <cstdint>

namespace hpc::support {

// Simple wrapper around steady_clock to keep dependencies light.
// This can be extended later with TSC-based timing if desired.

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

inline std::uint64_t to_nanoseconds(std::chrono::nanoseconds ns) noexcept
{
    return static_cast<std::uint64_t>(ns.count());
}

} // namespace hpc::support

