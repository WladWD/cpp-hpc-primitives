#pragma once

namespace hpc::support {

struct noncopyable {
    noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(noncopyable&&) = default;
};

} // namespace hpc::support

