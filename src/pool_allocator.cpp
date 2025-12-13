#include <hpc/core/pool_allocator.hpp>

#include <cassert>
#include <cstring>

namespace hpc::core {

fixed_pool::fixed_pool(std::size_t element_size, std::size_t element_count)
    : element_size_(element_size < sizeof(node) ? sizeof(node) : element_size)
    , element_count_(element_count)
{
    if (element_count_ == 0) {
        storage_ = nullptr;
        free_list_ = nullptr;
        return;
    }

    storage_ = static_cast<std::byte*>(::operator new(element_size_ * element_count_));
    free_list_ = nullptr;

    for (std::size_t i = 0; i < element_count_; ++i) {
        auto* n = reinterpret_cast<node*>(storage_ + i * element_size_);
        n->next = free_list_;
        free_list_ = n;
    }
}

fixed_pool::~fixed_pool()
{
    ::operator delete(storage_);
}

void* fixed_pool::allocate()
{
    if (!free_list_) {
        return nullptr;
    }
    node* n = free_list_;
    free_list_ = n->next;
    return n;
}

void fixed_pool::deallocate(void* p) noexcept
{
    if (!p) return;
    auto* n = static_cast<node*>(p);
    n->next = free_list_;
    free_list_ = n;
}

} // namespace hpc::core

