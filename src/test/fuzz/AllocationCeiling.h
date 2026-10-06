/*
 * Project Ambrose by Imjustchico
 * Aborts a fuzz run on any single allocation above the frame limit while an input is being processed.
 */

#ifndef AMBROSE_ALLOCATIONCEILING_H
#define AMBROSE_ALLOCATIONCEILING_H

#include "Frame.h"

#include <sanitizer/allocator_interface.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>

namespace AllocationCeiling
{
    inline std::atomic<bool> Armed{ false };

    inline void OnMalloc(volatile void const*, std::size_t size)
    {
        if (size > FrameLimits::DefaultMaxFrameSize && Armed.load(std::memory_order_relaxed))
            std::abort();
    }

    inline void OnFree(volatile void const*) {}

    inline void Install()
    {
        __sanitizer_install_malloc_and_free_hooks(OnMalloc, OnFree);
    }

    struct Scope
    {
        Scope() { Armed.store(true, std::memory_order_relaxed); }
        ~Scope() { Armed.store(false, std::memory_order_relaxed); }
        Scope(Scope const&) = delete;
        Scope& operator=(Scope const&) = delete;
    };
}

#endif
