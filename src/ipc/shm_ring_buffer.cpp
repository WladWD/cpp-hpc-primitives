#include <hpc/ipc/shm_ring_buffer.hpp>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hpc::ipc {

shm_region::shm_region(const shm_ring_config& cfg)
    : name_(cfg.name)
{
    int flags = cfg.create ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR;
    fd_ = ::shm_open(name_.c_str(), flags, 0600);
    if (fd_ == -1 && cfg.create && errno == EEXIST) {
        // If already exists and create requested, try to open existing.
        fd_ = ::shm_open(name_.c_str(), O_RDWR, 0600);
    }
    if (fd_ == -1) {
        throw std::runtime_error("shm_open failed: " + std::string(std::strerror(errno)));
    }

    // For simplicity, size is provided by caller; we store it in shm_ring_config
    // users are responsible for consistent configuration across processes.
    size_ = cfg.capacity;
    if (cfg.create) {
        if (::ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
            ::close(fd_);
            throw std::runtime_error("ftruncate failed");
        }
        owner_ = true;
    }

    addr_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr_ == MAP_FAILED) {
        ::close(fd_);
        throw std::runtime_error("mmap failed");
    }
}

shm_region::~shm_region()
{
    if (addr_ && addr_ != MAP_FAILED) {
        ::munmap(addr_, size_);
    }
    if (fd_ != -1) {
        ::close(fd_);
    }
    if (owner_) {
        ::shm_unlink(name_.c_str());
    }
}

} // namespace hpc::ipc

