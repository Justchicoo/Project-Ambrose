/*
 * Project Ambrose by Imjustchico
 * The bit positions of the client's ObjectProperty property flags and a helper that tests one in a flag word.
 */

#ifndef AMBROSE_PROPERTYFLAGS_H
#define AMBROSE_PROPERTYFLAGS_H

#include "Types.h"

enum class PropertyFlag : uint8
{
    Save = 0,
    Copy = 1,
    Public = 2,
    Transmit = 3,
    AuthorityTransmit = 4,
    Persistent = 5,
    Deprecated = 6,
    NoScript = 7,
    DirtyEncode = 8,
    Blob = 9,
    Immutable = 16,
    FileName = 17,
    Color = 18,
    Bits = 20,
    Enum = 21,
    Localized = 22,
    StringKey = 23,
    ObjectId = 24,
    ReferenceId = 25,
    ObjectName = 27,
    HasBaseClass = 28
};

namespace PropertyFlags
{
    constexpr uint32 Bit(PropertyFlag flag) noexcept
    {
        return uint32{ 1 } << static_cast<uint8>(flag);
    }

    constexpr bool Has(uint32 flags, PropertyFlag flag) noexcept
    {
        return (flags & Bit(flag)) != 0;
    }
}

#endif
