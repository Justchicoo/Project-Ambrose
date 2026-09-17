/*
 * Project Ambrose by Imjustchico
 * The guest process heap: a deterministic bump allocator in one mapped region that never reuses memory, remembers each block's size for reallocation and size queries, zero-fills on request, and throws when the region is exhausted.
 */

#ifndef AMBROSE_GUESTHEAP_H
#define AMBROSE_GUESTHEAP_H

#include "Types.h"

#include <optional>
#include <unordered_map>

class Machine;

class GuestHeap
{
public:
    static constexpr uint64 Alignment = 16;
    static constexpr uint64 BlockGap = 16;

    GuestHeap(Machine& machine, uint64 base, uint64 size);

    uint64 Allocate(uint64 size, bool zeroed);
    uint64 Reallocate(uint64 address, uint64 size, bool zeroed);
    bool Free(uint64 address);
    std::optional<uint64> SizeOf(uint64 address) const;
    bool Contains(uint64 address) const noexcept;

    uint64 GetBase() const noexcept;
    uint64 GetTop() const noexcept;
    uint64 GetLimit() const noexcept;
    uint64 GetAllocationCount() const noexcept;

private:
    Machine& _machine;
    uint64 _base;
    uint64 _limit;
    uint64 _top;
    uint64 _allocations = 0;
    std::unordered_map<uint64, uint64> _sizes;
};

#endif
