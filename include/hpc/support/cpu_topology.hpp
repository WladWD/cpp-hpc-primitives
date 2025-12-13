#pragma once

#include <thread>

namespace hpc::support {

// Attempt to pin the given thread to the specified core.
// On platforms where this is not supported, this is a no-op and returns false.
bool pin_thread_to_core(std::thread& thread, unsigned core_id) noexcept;

} // namespace hpc::support

