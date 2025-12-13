#include <hpc/support/cpu_topology.hpp>

#if defined(__linux__)
#include <pthread.h>
#endif

namespace hpc::support {

bool pin_thread_to_core(std::thread& thread, unsigned core_id) noexcept
{
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    auto handle = thread.native_handle();
    int rc = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
    return rc == 0;
#else
    (void)thread;
    (void)core_id;
    return false;
#endif
}

} // namespace hpc::support

