/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes Base64, rejecting invalid characters, misplaced padding, and non-zero pad bits.
 */

#include "Base64.h"

#include <array>

namespace
{
    constexpr std::string_view StandardCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    constexpr std::string_view UrlSafeCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string_view CharactersFor(Base64::Alphabet alphabet)
    {
        return alphabet == Base64::Alphabet::UrlSafe ? UrlSafeCharacters : StandardCharacters;
    }

    int ValueOf(char c, std::string_view characters)
    {
        std::size_t const position = characters.find(c);
        return position == std::string_view::npos ? -1 : static_cast<int>(position);
    }
}

std::string Base64::Encode(std::span<uint8 const> bytes, Alphabet alphabet, Padding padding)
{
    std::string_view const characters = CharactersFor(alphabet);
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t index = 0;
    for (; index + 3 <= bytes.size(); index += 3)
    {
        uint32 const block = (uint32(bytes[index]) << 16) | (uint32(bytes[index + 1]) << 8) | bytes[index + 2];
        out.push_back(characters[(block >> 18) & 0x3F]);
        out.push_back(characters[(block >> 12) & 0x3F]);
        out.push_back(characters[(block >> 6) & 0x3F]);
        out.push_back(characters[block & 0x3F]);
    }
    std::size_t const remaining = bytes.size() - index;
    if (remaining == 1)
    {
        uint32 const block = uint32(bytes[index]) << 16;
        out.push_back(characters[(block >> 18) & 0x3F]);
        out.push_back(characters[(block >> 12) & 0x3F]);
        if (padding == Padding::Required)
            out.append("==");
    }
    else if (remaining == 2)
    {
        uint32 const block = (uint32(bytes[index]) << 16) | (uint32(bytes[index + 1]) << 8);
        out.push_back(characters[(block >> 18) & 0x3F]);
        out.push_back(characters[(block >> 12) & 0x3F]);
        out.push_back(characters[(block >> 6) & 0x3F]);
        if (padding == Padding::Required)
            out.push_back('=');
    }
    return out;
}

std::optional<std::vector<uint8>> Base64::Decode(std::string_view text, Alphabet alphabet, Padding padding)
{
    std::size_t padCount = 0;
    if (padding == Padding::Required)
    {
        if (text.size() % 4 != 0)
            return std::nullopt;
        while (padCount < 2 && padCount < text.size() && text[text.size() - 1 - padCount] == '=')
            ++padCount;
        text.remove_suffix(padCount);
    }
    else if (text.find('=') != std::string_view::npos)
    {
        return std::nullopt;
    }

    std::size_t const tail = text.size() % 4;
    if (tail == 1 || (padding == Padding::Required && padCount != 0 && tail != 4 - padCount))
        return std::nullopt;

    std::string_view const characters = CharactersFor(alphabet);
    std::vector<uint8> out;
    out.reserve(text.size() * 3 / 4);
    uint32 accumulator = 0;
    int bits = 0;
    for (char c : text)
    {
        int const value = ValueOf(c, characters);
        if (value < 0)
            return std::nullopt;
        accumulator = (accumulator << 6) | static_cast<uint32>(value);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<uint8>((accumulator >> bits) & 0xFF));
        }
    }
    if (bits > 0 && (accumulator & ((uint32(1) << bits) - 1)) != 0)
        return std::nullopt;
    return out;
}
