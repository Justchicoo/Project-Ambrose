/*
 * Project Ambrose by Imjustchico
 * A permID is FNV-1a over the zone path, a separator, and the template and object ids in little-endian order, so it is the same number on every machine and every run for the same placed object, and it is never zero, because zero is what an object with no permID carries. Runtime GIDs come from one generator that starts at RuntimeBase.
 */

#include "ObjectGuid.h"

namespace
{
    constexpr uint64 Offset = 14695981039346656037ull;
    constexpr uint64 Prime = 1099511628211ull;

    uint64 Mix(uint64 hash, uint8 byte) noexcept
    {
        return (hash ^ byte) * Prime;
    }
}

uint64 ObjectGuid::PermId(std::string_view zonePath, uint64 templateId, uint32 objectId) noexcept
{
    uint64 hash = Offset;
    for (char const c : zonePath)
        hash = Mix(hash, static_cast<uint8>(c));
    hash = Mix(hash, 0);
    for (int shift = 0; shift < 64; shift += 8)
        hash = Mix(hash, static_cast<uint8>(templateId >> shift));
    for (int shift = 0; shift < 32; shift += 8)
        hash = Mix(hash, static_cast<uint8>(objectId >> shift));
    return hash == 0 ? 1 : hash;
}

GuidGenerator& ObjectGuid::RuntimeGuids() noexcept
{
    static GuidGenerator guids(RuntimeBase);
    return guids;
}

std::optional<uint64> ObjectGuid::NextRuntime() noexcept
{
    return RuntimeGuids().Generate();
}

bool ObjectGuid::IsRuntime(uint64 guid) noexcept
{
    return guid >= RuntimeBase;
}
