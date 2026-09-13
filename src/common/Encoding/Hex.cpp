/*
 * Project Ambrose by Imjustchico
 * Implements hex encoding, strict hex decoding, and sixteen-byte hexdump lines with an ASCII column.
 */

#include "Hex.h"

#include <fmt/format.h>

namespace
{
    int NibbleOf(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }
}

std::string Hex::Encode(std::span<uint8 const> bytes, Case letterCase)
{
    std::string_view const digits = letterCase == Case::Upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8 byte : bytes)
    {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

std::optional<std::vector<uint8>> Hex::Decode(std::string_view text)
{
    if (text.size() % 2 != 0)
        return std::nullopt;
    std::vector<uint8> out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2)
    {
        int const high = NibbleOf(text[i]);
        int const low = NibbleOf(text[i + 1]);
        if (high < 0 || low < 0)
            return std::nullopt;
        out.push_back(static_cast<uint8>((high << 4) | low));
    }
    return out;
}

std::string Hex::Dump(std::span<uint8 const> bytes, std::size_t startOffset)
{
    std::string out;
    for (std::size_t lineStart = 0; lineStart < bytes.size(); lineStart += 16)
    {
        std::size_t const count = bytes.size() - lineStart < 16 ? bytes.size() - lineStart : 16;
        out += fmt::format("{:08x}  ", startOffset + lineStart);
        for (std::size_t i = 0; i < 16; ++i)
        {
            if (i < count)
                out += fmt::format("{:02x} ", bytes[lineStart + i]);
            else
                out += "   ";
            if (i == 7)
                out += ' ';
        }
        out += " |";
        for (std::size_t i = 0; i < count; ++i)
        {
            uint8 const byte = bytes[lineStart + i];
            out += (byte >= 0x20 && byte <= 0x7E) ? static_cast<char>(byte) : '.';
        }
        out += "|\n";
    }
    return out;
}
