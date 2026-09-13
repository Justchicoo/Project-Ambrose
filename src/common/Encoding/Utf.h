/*
 * Project Ambrose by Imjustchico
 * Strict UTF-8 and UTF-16 conversion with an explicit replace-or-reject policy for invalid input.
 */

#ifndef AMBROSE_UTF_H
#define AMBROSE_UTF_H

#include "Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Utf
{
    inline constexpr char32_t ReplacementCharacter = U'\uFFFD';

    enum class InvalidPolicy
    {
        ReplaceWithU_FFFD,
        Reject
    };

    std::optional<std::u16string> Utf8ToUtf16(std::string_view utf8, InvalidPolicy policy);
    std::optional<std::string> Utf16ToUtf8(std::u16string_view utf16, InvalidPolicy policy);

    std::optional<std::u16string> Utf16LEBytesToString(std::span<uint8 const> bytes, InvalidPolicy policy);
    std::vector<uint8> StringToUtf16LEBytes(std::u16string_view utf16);

    bool IsValidUtf8(std::string_view utf8);
    bool IsValidUtf16(std::u16string_view utf16);
}

#endif
