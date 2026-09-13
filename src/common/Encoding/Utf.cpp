/*
 * Project Ambrose by Imjustchico
 * Decodes UTF-8 by the Unicode maximal subpart rule and UTF-16 surrogate pairs, re-encoding code points exactly.
 */

#include "Utf.h"

namespace
{
    struct Decoded
    {
        char32_t CodePoint;
        std::size_t Consumed;
        bool Valid;
    };

    constexpr bool InRange(uint8 value, uint8 low, uint8 high)
    {
        return value >= low && value <= high;
    }

    Decoded DecodeUtf8(std::string_view text, std::size_t index)
    {
        uint8 const lead = static_cast<uint8>(text[index]);
        if (lead < 0x80)
            return { lead, 1, true };

        std::size_t continuations = 0;
        uint8 secondLow = 0x80;
        uint8 secondHigh = 0xBF;
        char32_t codePoint = 0;
        if (InRange(lead, 0xC2, 0xDF))
        {
            continuations = 1;
            codePoint = lead & 0x1Fu;
        }
        else if (InRange(lead, 0xE0, 0xEF))
        {
            continuations = 2;
            codePoint = lead & 0x0Fu;
            if (lead == 0xE0)
                secondLow = 0xA0;
            else if (lead == 0xED)
                secondHigh = 0x9F;
        }
        else if (InRange(lead, 0xF0, 0xF4))
        {
            continuations = 3;
            codePoint = lead & 0x07u;
            if (lead == 0xF0)
                secondLow = 0x90;
            else if (lead == 0xF4)
                secondHigh = 0x8F;
        }
        else
        {
            return { 0, 1, false };
        }

        std::size_t consumed = 1;
        for (std::size_t i = 0; i < continuations; ++i)
        {
            std::size_t const position = index + consumed;
            if (position >= text.size())
                return { 0, consumed, false };
            uint8 const byte = static_cast<uint8>(text[position]);
            uint8 const low = i == 0 ? secondLow : uint8(0x80);
            uint8 const high = i == 0 ? secondHigh : uint8(0xBF);
            if (!InRange(byte, low, high))
                return { 0, consumed, false };
            codePoint = (codePoint << 6) | (byte & 0x3Fu);
            ++consumed;
        }
        return { codePoint, consumed, true };
    }

    void AppendUtf16(std::u16string& out, char32_t codePoint)
    {
        if (codePoint < 0x10000)
        {
            out.push_back(static_cast<char16_t>(codePoint));
            return;
        }
        char32_t const offset = codePoint - 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (offset >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (offset & 0x3FF)));
    }

    void AppendUtf8(std::string& out, char32_t codePoint)
    {
        if (codePoint < 0x80)
        {
            out.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    constexpr bool IsHighSurrogate(char16_t unit)
    {
        return unit >= 0xD800 && unit <= 0xDBFF;
    }

    constexpr bool IsLowSurrogate(char16_t unit)
    {
        return unit >= 0xDC00 && unit <= 0xDFFF;
    }
}

std::optional<std::u16string> Utf::Utf8ToUtf16(std::string_view utf8, InvalidPolicy policy)
{
    std::u16string result;
    result.reserve(utf8.size());
    std::size_t index = 0;
    while (index < utf8.size())
    {
        Decoded const decoded = DecodeUtf8(utf8, index);
        if (!decoded.Valid)
        {
            if (policy == InvalidPolicy::Reject)
                return std::nullopt;
            AppendUtf16(result, ReplacementCharacter);
        }
        else
        {
            AppendUtf16(result, decoded.CodePoint);
        }
        index += decoded.Consumed;
    }
    return result;
}

std::optional<std::string> Utf::Utf16ToUtf8(std::u16string_view utf16, InvalidPolicy policy)
{
    std::string result;
    result.reserve(utf16.size());
    for (std::size_t index = 0; index < utf16.size(); ++index)
    {
        char16_t const unit = utf16[index];
        char32_t codePoint = unit;
        if (IsHighSurrogate(unit) && index + 1 < utf16.size() && IsLowSurrogate(utf16[index + 1]))
        {
            codePoint = 0x10000 + ((char32_t(unit) - 0xD800) << 10) + (char32_t(utf16[index + 1]) - 0xDC00);
            ++index;
        }
        else if (IsHighSurrogate(unit) || IsLowSurrogate(unit))
        {
            if (policy == InvalidPolicy::Reject)
                return std::nullopt;
            codePoint = ReplacementCharacter;
        }
        AppendUtf8(result, codePoint);
    }
    return result;
}

std::optional<std::u16string> Utf::Utf16LEBytesToString(std::span<uint8 const> bytes, InvalidPolicy policy)
{
    if (bytes.size() % 2 != 0 && policy == InvalidPolicy::Reject)
        return std::nullopt;
    std::u16string result;
    result.reserve(bytes.size() / 2 + 1);
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2)
        result.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
    if (bytes.size() % 2 != 0)
        result.push_back(static_cast<char16_t>(ReplacementCharacter));
    return result;
}

std::vector<uint8> Utf::StringToUtf16LEBytes(std::u16string_view utf16)
{
    std::vector<uint8> bytes;
    bytes.reserve(utf16.size() * 2);
    for (char16_t unit : utf16)
    {
        bytes.push_back(static_cast<uint8>(unit & 0xFF));
        bytes.push_back(static_cast<uint8>(unit >> 8));
    }
    return bytes;
}

bool Utf::IsValidUtf8(std::string_view utf8)
{
    std::size_t index = 0;
    while (index < utf8.size())
    {
        Decoded const decoded = DecodeUtf8(utf8, index);
        if (!decoded.Valid)
            return false;
        index += decoded.Consumed;
    }
    return true;
}

bool Utf::IsValidUtf16(std::u16string_view utf16)
{
    return Utf16ToUtf8(utf16, InvalidPolicy::Reject).has_value();
}
