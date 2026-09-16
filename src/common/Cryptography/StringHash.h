/*
 * Project Ambrose by Imjustchico
 * The client's string hashes, usable at compile time: the KI string hash behind class hashes and string IDs, djb2, and the property hash that combines a type's KI hash with its name's djb2.
 */

#ifndef AMBROSE_STRINGHASH_H
#define AMBROSE_STRINGHASH_H

#include "Types.h"

#include <string_view>

namespace StringHash
{
    constexpr uint32 KiStringHash(std::string_view text) noexcept
    {
        uint32 result = 0;
        int32 shift = 0;
        int32 overflowShift = 32;
        for (char const character : text)
        {
            int32 const value = static_cast<int32>(static_cast<uint8>(character)) - 32;
            result ^= static_cast<uint32>(value) << shift;
            if (shift > 24)
            {
                result ^= static_cast<uint32>(value >> overflowShift);
                if (shift >= 27)
                {
                    shift -= 32;
                    overflowShift += 32;
                }
            }
            shift += 5;
            overflowShift -= 5;
        }
        return static_cast<int32>(result) < 0 ? 0u - result : result;
    }

    constexpr uint32 StringId(std::string_view text) noexcept
    {
        return KiStringHash(text);
    }

    constexpr uint32 Djb2(std::string_view text) noexcept
    {
        uint32 hash = 5381;
        for (char const character : text)
            hash = (hash << 5) + hash + static_cast<uint8>(character);
        return hash;
    }

    constexpr uint32 PropertyHash(std::string_view typeName, std::string_view propertyName) noexcept
    {
        return KiStringHash(typeName) + (Djb2(propertyName) & 0x7FFFFFFFu);
    }
}

#endif
