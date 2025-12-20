#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <thread>

#include <hpc/ipc/shm_ring_buffer.hpp>

namespace {

using clock = std::chrono::steady_clock;

struct Message {
    std::uint64_t seq;
    std::uint64_t timestamp_ns;
    std::uint8_t  payload[48];
};

constexpr const char* kShmName   = "/hpc_shm_spsc_ring";
constexpr std::size_t kCapacity  = 1024; // number of messages
constexpr std::chrono::milliseconds kSleep{1};

volatile std::sig_atomic_t g_stop = 0;

void signal_handler(int) { g_stop = 1; }

} // namespace

int main()
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        hpc::ipc::shm_ring_config cfg;
        cfg.name     = kShmName;
        cfg.capacity = kCapacity; // slots; shm_spsc_ring_buffer computes bytes
        cfg.create   = true;

        hpc::ipc::shm_spsc_ring_buffer<Message> ring{cfg};

        std::uint64_t seq = 0;
        while (!g_stop) {
            Message msg{};
            msg.seq = seq;
            auto now = clock::now().time_since_epoch();
            msg.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            std::memset(msg.payload, 0, sizeof(msg.payload));

            if (!ring.try_push(msg)) {
                // Simple backpressure: drop oldest element.
                Message dropped;
                if (ring.try_pop(dropped)) {
                    (void)dropped;
                    ring.try_push(msg);
                }
            }

            if ((seq % 1000) == 0) {
                std::cout << "published seq=" << seq << '\n';
            }

            ++seq;
            std::this_thread::sleep_for(kSleep);
        }
    } catch (const std::exception& ex) {
        std::cerr << "shm_publisher exception: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
