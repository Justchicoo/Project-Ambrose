/*
 * Project Ambrose by Imjustchico
 * Writes five bits to a character, most significant first, and reads them back after folding the text to its canonical spelling, refusing a character outside the alphabet, a length no byte count produces and leftover bits that are not zero, so every byte string has exactly one written form.
 */

#include "Base32.h"

namespace
{
    constexpr std::string_view Rfc4648Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    constexpr std::string_view CrockfordCharacters = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

    char Upper(char c) noexcept
    {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }
}

std::string_view Base32::Characters(Alphabet alphabet) noexcept
{
    return alphabet == Alphabet::Crockford ? CrockfordCharacters : Rfc4648Characters;
}

std::string Base32::Encode(std::span<uint8 const> bytes, Alphabet alphabet)
{
    std::string_view const characters = Characters(alphabet);
    std::string out;
    out.reserve((bytes.size() * 8 + 4) / 5);
    uint32 buffer = 0;
    int bits = 0;
    for (uint8 const byte : bytes)
    {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 5)
        {
            bits -= 5;
            out.push_back(characters[(buffer >> bits) & 0x1F]);
        }
        buffer &= (1u << bits) - 1;
    }
    if (bits > 0)
        out.push_back(characters[(buffer << (5 - bits)) & 0x1F]);
    return out;
}

std::optional<std::string> Base32::Canonical(std::string_view text, Alphabet alphabet)
{
    std::string_view const characters = Characters(alphabet);
    std::string out;
    out.reserve(text.size());
    if (alphabet == Alphabet::Rfc4648)
    {
        while (!text.empty() && text.back() == '=')
            text.remove_suffix(1);
        for (char const c : text)
        {
            char const upper = Upper(c);
            if (characters.find(upper) == std::string_view::npos)
                return std::nullopt;
            out.push_back(upper);
        }
        return out;
    }
    for (char const c : text)
    {
        if (c == '-' || c == ' ')
            continue;
        char upper = Upper(c);
        if (upper == 'O')
            upper = '0';
        else if (upper == 'I' || upper == 'L')
            upper = '1';
        if (characters.find(upper) == std::string_view::npos)
            return std::nullopt;
        out.push_back(upper);
    }
    return out;
}

std::optional<std::vector<uint8>> Base32::Decode(std::string_view text, Alphabet alphabet)
{
    std::optional<std::string> const canonical = Canonical(text, alphabet);
    if (!canonical)
        return std::nullopt;
    std::string_view const characters = Characters(alphabet);
    std::vector<uint8> out;
    out.reserve(canonical->size() * 5 / 8);
    uint32 buffer = 0;
    int bits = 0;
    for (char const c : *canonical)
    {
        buffer = (buffer << 5) | static_cast<uint32>(characters.find(c));
        bits += 5;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<uint8>((buffer >> bits) & 0xFF));
        }
        buffer &= (1u << bits) - 1;
    }
    if (bits >= 5 || buffer != 0)
        return std::nullopt;
    return out;
}
