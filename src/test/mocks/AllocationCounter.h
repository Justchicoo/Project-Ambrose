/*
 * Project Ambrose by Imjustchico
 * Counts heap allocations made on the current thread while a scope is active, with the largest and the total bytes requested, for allocation tests.
 */

#ifndef AMBROSE_ALLOCATIONCOUNTER_H
#define AMBROSE_ALLOCATIONCOUNTER_H

#include <cstddef>

class AllocationScope
{
public:
    AllocationScope();
    ~AllocationScope();

    AllocationScope(AllocationScope const&) = delete;
    AllocationScope& operator=(AllocationScope const&) = delete;

    std::size_t GetCount() const;
    std::size_t GetLargest() const;
    std::size_t GetTotal() const;

    static bool IsSupported();
};

#endif
