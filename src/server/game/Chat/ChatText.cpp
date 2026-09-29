/*
 * Project Ambrose by Imjustchico
 * Reads the count and exactly that many code units, so a line with bytes left over or missing is refused, and writes the count and the units, a text longer than the count can hold cut at its limit.
 */

#include "ChatText.h"

#include <algorithm>

std::optional<std::u16string> ChatText::Read(std::string_view bytes)
{
    if (bytes.size() < 2)
        return std::nullopt;
    std::size_t const units = static_cast<uint8>(bytes[0]) | (static_cast<std::size_t>(static_cast<uint8>(bytes[1])) << 8);
    if (bytes.size() != 2 + units * 2)
        return std::nullopt;
    std::u16string text;
    text.reserve(units);
    for (std::size_t unit = 0; unit < units; ++unit)
        text.push_back(static_cast<char16_t>(static_cast<uint8>(bytes[2 + unit * 2]) | (static_cast<uint16>(static_cast<uint8>(bytes[3 + unit * 2])) << 8)));
    return text;
}

std::string ChatText::Write(std::u16string_view text)
{
    std::size_t const units = std::min(text.size(), MaxUnits);
    std::string bytes;
    bytes.reserve(2 + units * 2);
    bytes.push_back(static_cast<char>(units & 0xFF));
    bytes.push_back(static_cast<char>(units >> 8));
    for (std::size_t unit = 0; unit < units; ++unit)
    {
        bytes.push_back(static_cast<char>(text[unit] & 0xFF));
        bytes.push_back(static_cast<char>(text[unit] >> 8));
    }
    return bytes;
}
