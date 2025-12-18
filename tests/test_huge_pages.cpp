#include <gtest/gtest.h>

#include <hpc/support/huge_pages.hpp>

namespace {

TEST(HugePages, BasicAllocationAndFree)
{
    constexpr std::size_t kSize = 1 << 20; // 1 MiB
    auto region = hpc::support::huge_page_alloc(kSize);

    if (region.ptr) {
        EXPECT_GE(region.size, kSize);
        EXPECT_NE(region.align, 0u);
    } else {
        EXPECT_EQ(region.size, 0u);
        EXPECT_EQ(region.align, 0u);
    }

    hpc::support::huge_page_free(region);
}

} // namespace
