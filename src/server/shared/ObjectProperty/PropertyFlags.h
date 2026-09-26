/*
 * Project Ambrose by Imjustchico
 * The bit positions of the client's ObjectProperty property flags, the name each is printed and asked for by, and helpers that test one in a flag word and find one by its name whatever its case.
 */

#ifndef AMBROSE_PROPERTYFLAGS_H
#define AMBROSE_PROPERTYFLAGS_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

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

    inline constexpr std::array<std::pair<PropertyFlag, std::string_view>, 21> Names{ {
        { PropertyFlag::Save, "Save" },
        { PropertyFlag::Copy, "Copy" },
        { PropertyFlag::Public, "Public" },
        { PropertyFlag::Transmit, "Transmit" },
        { PropertyFlag::AuthorityTransmit, "AuthorityTransmit" },
        { PropertyFlag::Persistent, "Persistent" },
        { PropertyFlag::Deprecated, "Deprecated" },
        { PropertyFlag::NoScript, "NoScript" },
        { PropertyFlag::DirtyEncode, "DirtyEncode" },
        { PropertyFlag::Blob, "Blob" },
        { PropertyFlag::Immutable, "Immutable" },
        { PropertyFlag::FileName, "FileName" },
        { PropertyFlag::Color, "Color" },
        { PropertyFlag::Bits, "Bits" },
        { PropertyFlag::Enum, "Enum" },
        { PropertyFlag::Localized, "Localized" },
        { PropertyFlag::StringKey, "StringKey" },
        { PropertyFlag::ObjectId, "ObjectId" },
        { PropertyFlag::ReferenceId, "ReferenceId" },
        { PropertyFlag::ObjectName, "ObjectName" },
        { PropertyFlag::HasBaseClass, "HasBaseClass" },
    } };

    constexpr std::optional<PropertyFlag> FromName(std::string_view name) noexcept
    {
        for (auto const& [flag, flagName] : Names)
        {
            if (flagName.size() != name.size())
                continue;
            bool same = true;
            for (std::size_t index = 0; index < name.size() && same; ++index)
            {
                char const left = flagName[index] >= 'A' && flagName[index] <= 'Z' ? static_cast<char>(flagName[index] - 'A' + 'a') : flagName[index];
                char const right = name[index] >= 'A' && name[index] <= 'Z' ? static_cast<char>(name[index] - 'A' + 'a') : name[index];
                same = left == right;
            }
            if (same)
                return flag;
        }
        return std::nullopt;
    }
}

#endif
