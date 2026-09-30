/*
 * Project Ambrose by Imjustchico
 * Reads a skipped value's bits through the bit reader fenced at the value's own length, a text as a compact length, one bit that says whether seven or 31 bits of length follow, then that many printable bytes, and gives every reading that uses every bit, followed by the bytes in hex.
 */

#include "SkippedValue.h"
#include "BitReader.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <bit>
#include <optional>
#include <ranges>

namespace
{
    std::optional<std::string> ReadCompactText(BitReader& reader)
    {
        bool const wide = reader.ReadBit();
        uint64 const length = reader.ReadBits(wide ? 31 : 7);
        if (reader.Failed() || length > reader.GetRemainingBits() / 8)
            return std::nullopt;
        std::span<uint8 const> const bytes = reader.ReadBytes(static_cast<std::size_t>(length));
        if (reader.Failed() || !std::ranges::all_of(bytes, [](uint8 c) { return c >= 0x20 && c < 0x7F; }))
            return std::nullopt;
        return std::string(bytes.begin(), bytes.end());
    }

    BitReader Fenced(uint64 bits, std::span<uint8 const> value)
    {
        BitReader reader(value);
        reader.SetLimit(static_cast<std::size_t>(std::min<uint64>(bits, value.size() * uint64{ 8 })));
        return reader;
    }
}

std::vector<std::string> SkippedValue::Readings(uint64 bits, std::span<uint8 const> value)
{
    std::vector<std::string> readings;
    if (value.size() * uint64{ 8 } < bits || value.empty())
        return readings;
    if (bits == 1)
        readings.push_back(fmt::format("bool {}", value.front() & 1 ? "true" : "false"));
    if (bits == 32)
    {
        BitReader reader = Fenced(bits, value);
        uint32 const word = reader.Read<uint32>();
        readings.push_back(fmt::format("int {}, unsigned int {}, float {}", static_cast<int32>(word), word, std::bit_cast<float>(word)));
    }
    if (bits == 64)
    {
        BitReader reader = Fenced(bits, value);
        uint64 const word = reader.Read<uint64>();
        readings.push_back(fmt::format("unsigned __int64 {}, double {}", word, std::bit_cast<double>(word)));
    }
    if (bits == 96)
    {
        BitReader reader = Fenced(bits, value);
        float const x = reader.Read<float>();
        float const y = reader.Read<float>();
        float const z = reader.Read<float>();
        readings.push_back(fmt::format("three floats {} {} {}", x, y, z));
    }
    if (bits % 8 == 0)
    {
        BitReader text = Fenced(bits, value);
        if (std::optional<std::string> const one = ReadCompactText(text); one && text.GetRemainingBits() == 0)
            readings.push_back(fmt::format("std::string \"{}\"", *one));
        BitReader list = Fenced(bits, value);
        bool const wide = list.ReadBit();
        uint64 const count = list.ReadBits(wide ? 31 : 7);
        std::vector<std::string> items;
        while (!list.Failed() && items.size() < count && list.GetRemainingBits() >= 8)
            if (std::optional<std::string> item = ReadCompactText(list))
                items.push_back(std::move(*item));
            else
                break;
        if (!list.Failed() && items.size() == count && list.GetRemainingBits() == 0)
            readings.push_back(fmt::format("a list of {} std::string [{}]", count, fmt::join(items | std::views::transform([](std::string const& item) { return fmt::format("\"{}\"", item); }), ", ")));
    }
    return readings;
}

std::optional<std::string> SkippedValue::Text(uint64 bits, std::span<uint8 const> value)
{
    if (bits % 8 != 0 || value.size() * uint64{ 8 } < bits || value.empty())
        return std::nullopt;
    BitReader reader = Fenced(bits, value);
    std::optional<std::string> text = ReadCompactText(reader);
    return text && reader.GetRemainingBits() == 0 ? text : std::nullopt;
}

std::string SkippedValue::Describe(uint64 bits, std::span<uint8 const> value)
{
    std::vector<std::string> const readings = Readings(bits, value);
    std::string const hex = fmt::format("{:02x}", fmt::join(value, " "));
    return readings.empty() ? fmt::format("bits {}", hex) : fmt::format("bits {}, reading as {}", hex, fmt::join(readings, "; or "));
}
