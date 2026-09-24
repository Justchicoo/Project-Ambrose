/*
 * Project Ambrose by Imjustchico
 * Proves the allocation counter observes real allocations, so no-allocation tests cannot pass vacuously.
 */

#include "AllocationCounter.h"

#include <gtest/gtest.h>

#include <memory>

TEST(AllocationCounterTest, ObservesAllocationsInsideScopeOnly)
{
    if (!AllocationScope::IsSupported())
        GTEST_SKIP() << "allocation counting is unavailable in this build";
    std::size_t count = 0;
    std::size_t largest = 0;
    {
        AllocationScope scope;
        auto const block = std::make_unique<char[]>(4096);
        block[0] = 1;
        count = scope.GetCount();
        largest = scope.GetLargest();
    }
    EXPECT_GE(count, 1u);
    EXPECT_GE(largest, 4096u);
    auto const outside = std::make_unique<char[]>(8192);
    outside[0] = 1;
    AllocationScope fresh;
    EXPECT_EQ(fresh.GetCount(), 0u);
}
