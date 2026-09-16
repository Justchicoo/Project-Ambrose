/*
 * Project Ambrose by Imjustchico
 * Advances the next id with compare-and-swap, so concurrent callers each claim a different value, and raises it past a resumed high-water mark the same way; handing out the largest 64-bit id wraps the next id to zero, which marks the generator exhausted.
 */

#include "GuidGenerator.h"

GuidGenerator::GuidGenerator(uint64 first) noexcept : _next(first == 0 ? 1 : first)
{
}

std::optional<uint64> GuidGenerator::Generate() noexcept
{
    uint64 current = _next.load(std::memory_order_acquire);
    do
    {
        if (current == 0)
            return std::nullopt;
    } while (!_next.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel));
    return current;
}

void GuidGenerator::Resume(uint64 highestUsed) noexcept
{
    uint64 const wanted = highestUsed + 1;
    uint64 current = _next.load(std::memory_order_acquire);
    while (current != 0 && (wanted == 0 || current < wanted) && !_next.compare_exchange_weak(current, wanted, std::memory_order_acq_rel))
    {
    }
}

std::optional<uint64> GuidGenerator::PeekNext() const noexcept
{
    uint64 const next = _next.load(std::memory_order_acquire);
    return next == 0 ? std::nullopt : std::optional<uint64>(next);
}
