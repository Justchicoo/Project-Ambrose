/*
 * Project Ambrose by Imjustchico
 * Hands out guest heap blocks by bumping an aligned top with a gap after each block, checks every size against the region's limit without overflowing, zero-fills requested blocks, and moves reallocated blocks by copying the kept bytes into a fresh block.
 */

#include "GuestHeap.h"
#include "Machine.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <vector>

namespace
{
    constexpr uint64 MaxAddress = std::numeric_limits<uint64>::max();
    constexpr std::size_t ZeroChunkSize = 0x10000;

    void FillZero(Machine& machine, uint64 address, uint64 size)
    {
        static constexpr std::array<uint8, ZeroChunkSize> zeros{};
        while (size > 0)
        {
            std::size_t const chunk = static_cast<std::size_t>(std::min<uint64>(size, zeros.size()));
            machine.Write(address, std::span<uint8 const>(zeros.data(), chunk));
            address += chunk;
            size -= chunk;
        }
    }
}

GuestHeap::GuestHeap(Machine& machine, uint64 base, uint64 size)
    : _machine(machine), _base(base), _limit(base), _top(base)
{
    if (size == 0)
        throw EmulationError(fmt::format("cannot create a guest heap at {:#x}: the size is zero", base));
    if (size > MaxAddress - base)
        throw EmulationError(fmt::format("cannot create a guest heap of {} bytes at {:#x}: the region passes the end of the address space", size, base));
    uint64 const misalignment = base % Alignment;
    uint64 const padding = misalignment == 0 ? 0 : Alignment - misalignment;
    if (padding >= size)
        throw EmulationError(fmt::format("cannot create a guest heap of {} bytes at {:#x}: no aligned address fits inside it", size, base));
    _machine.Map(base, size);
    _limit = base + size;
    _top = base + padding;
}

uint64 GuestHeap::Allocate(uint64 size, bool zeroed)
{
    size = std::max<uint64>(size, 1);
    uint64 const available = _limit - _top;
    if (size > available || BlockGap > available - size)
        throw EmulationError(fmt::format("the guest heap of {} bytes is exhausted: {} bytes were requested with {} bytes left after {} allocations", _limit - _base, size, available, _allocations));
    uint64 const end = _top + size + BlockGap;
    uint64 const remainder = end % Alignment;
    uint64 const padding = remainder == 0 ? 0 : Alignment - remainder;
    if (padding > _limit - end)
        throw EmulationError(fmt::format("the guest heap of {} bytes is exhausted: {} bytes were requested with {} bytes left after {} allocations", _limit - _base, size, available, _allocations));
    uint64 const address = _top;
    _sizes.emplace(address, size);
    _top = end + padding;
    ++_allocations;
    if (zeroed)
        FillZero(_machine, address, size);
    return address;
}

uint64 GuestHeap::Reallocate(uint64 address, uint64 size, bool zeroed)
{
    if (address == 0)
        return Allocate(size, zeroed);
    auto const old = _sizes.find(address);
    if (old == _sizes.end())
        throw EmulationError(fmt::format("cannot reallocate {:#x} to {} bytes: it is not a live block of the guest heap", address, size));
    uint64 const oldSize = old->second;
    uint64 const moved = Allocate(size, false);
    uint64 const kept = std::min(oldSize, std::max<uint64>(size, 1));
    std::vector<uint8> const bytes = _machine.ReadBytes(address, static_cast<std::size_t>(kept));
    _machine.Write(moved, bytes);
    if (zeroed && kept < size)
        FillZero(_machine, moved + kept, size - kept);
    _sizes.erase(address);
    return moved;
}

bool GuestHeap::Free(uint64 address)
{
    return _sizes.erase(address) > 0;
}

std::optional<uint64> GuestHeap::SizeOf(uint64 address) const
{
    auto const found = _sizes.find(address);
    if (found == _sizes.end())
        return std::nullopt;
    return found->second;
}

bool GuestHeap::Contains(uint64 address) const noexcept
{
    return address >= _base && address < _top;
}

uint64 GuestHeap::GetBase() const noexcept
{
    return _base;
}

uint64 GuestHeap::GetTop() const noexcept
{
    return _top;
}

uint64 GuestHeap::GetLimit() const noexcept
{
    return _limit;
}

uint64 GuestHeap::GetAllocationCount() const noexcept
{
    return _allocations;
}
