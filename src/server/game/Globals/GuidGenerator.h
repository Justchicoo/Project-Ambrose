/*
 * Project Ambrose by Imjustchico
 * Hands out 64-bit object ids from any number of threads without a lock, never twice, never zero, resuming above the highest id a store already holds and refusing once the 64-bit range is used up.
 */

#ifndef AMBROSE_GUIDGENERATOR_H
#define AMBROSE_GUIDGENERATOR_H

#include "Types.h"

#include <atomic>
#include <optional>

class GuidGenerator
{
public:
    explicit GuidGenerator(uint64 first = 1) noexcept;
    GuidGenerator(GuidGenerator const&) = delete;
    GuidGenerator& operator=(GuidGenerator const&) = delete;

    std::optional<uint64> Generate() noexcept;
    void Resume(uint64 highestUsed) noexcept;
    std::optional<uint64> PeekNext() const noexcept;

private:
    std::atomic<uint64> _next;
};

#endif
