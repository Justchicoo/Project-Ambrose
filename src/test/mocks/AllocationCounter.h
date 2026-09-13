/*
 * Project Ambrose by Imjustchico
 * Counts heap allocations made on the current thread while a scope is active, for no-allocation tests.
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

    static bool IsSupported();
};

#endif
