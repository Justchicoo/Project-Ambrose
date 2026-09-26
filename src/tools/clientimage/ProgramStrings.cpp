/*
 * Project Ambrose by Imjustchico
 * Scans each section that holds no code once, over the bytes its file holds, for ASCII runs and, at even offsets, UTF-16LE runs of printable characters, tab, line feed and carriage return among them, that a terminator ends, keeps them in address order, and finds them by a case-blind search of their text.
 */

#include "ProgramStrings.h"
#include "PeImage.h"
#include "StringHash.h"
#include "StringUtil.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <span>

namespace
{
    bool IsPrintable(uint32 value) noexcept
    {
        return (value >= 0x20 && value <= 0x7E) || value == '\t' || value == '\n' || value == '\r';
    }

    uint32 FileBackedSize(PeSection const& section) noexcept
    {
        return section.VirtualSize == 0 ? section.RawSize : std::min(section.RawSize, section.VirtualSize);
    }

    uint32 UnitAt(std::span<uint8 const> bytes, std::size_t unit) noexcept
    {
        return static_cast<uint32>(bytes[2 * unit]) | (static_cast<uint32>(bytes[2 * unit + 1]) << 8);
    }

    void ScanNarrow(std::span<uint8 const> bytes, uint64 start, std::vector<ProgramString>& found)
    {
        std::size_t index = 0;
        while (index < bytes.size())
        {
            if (!IsPrintable(bytes[index]))
            {
                ++index;
                continue;
            }
            std::size_t end = index;
            while (end < bytes.size() && IsPrintable(bytes[end]) && end - index < ProgramStrings::MaximumLength)
                ++end;
            if (end < bytes.size() && bytes[end] == 0 && end - index >= ProgramStrings::MinimumLength)
                found.push_back({ start + index, static_cast<uint32>(end - index), false, std::string(reinterpret_cast<char const*>(bytes.data() + index), end - index) });
            index = end + 1;
        }
    }

    void ScanWide(std::span<uint8 const> bytes, uint64 start, std::vector<ProgramString>& found)
    {
        std::size_t const units = bytes.size() / 2;
        std::size_t index = 0;
        while (index < units)
        {
            if (!IsPrintable(UnitAt(bytes, index)))
            {
                ++index;
                continue;
            }
            std::string text;
            std::size_t end = index;
            while (end < units && IsPrintable(UnitAt(bytes, end)) && end - index < ProgramStrings::MaximumLength)
                text.push_back(static_cast<char>(UnitAt(bytes, end++)));
            if (end < units && UnitAt(bytes, end) == 0 && end - index >= ProgramStrings::MinimumLength)
                found.push_back({ start + 2 * index, static_cast<uint32>(2 * (end - index)), true, std::move(text) });
            index = end + 1;
        }
    }
}

ProgramStrings::ProgramStrings(PeImage const& image)
{
    for (PeSection const& section : image.GetSections())
    {
        if (section.IsExecutable())
            continue;
        std::span<uint8 const> const bytes = image.ReadRva(section.VirtualAddress, FileBackedSize(section));
        uint64 const start = image.GetImageBase() + section.VirtualAddress;
        ScanNarrow(bytes, start, _strings);
        ScanWide(bytes, start, _strings);
    }
    std::sort(_strings.begin(), _strings.end(), [](ProgramString const& left, ProgramString const& right) { return left.Address < right.Address; });
}

std::vector<ProgramString const*> ProgramStrings::Find(std::string_view text) const
{
    std::string const wanted = Ambrose::ToLower(text);
    std::vector<ProgramString const*> matches;
    for (ProgramString const& string : _strings)
        if (Ambrose::ToLower(string.Text).find(wanted) != std::string::npos)
            matches.push_back(&string);
    return matches;
}

ProgramString const* ProgramStrings::Containing(uint64 address) const
{
    auto const after = std::upper_bound(_strings.begin(), _strings.end(), address, [](uint64 value, ProgramString const& string) { return value < string.Address; });
    if (after == _strings.begin())
        return nullptr;
    ProgramString const& candidate = *std::prev(after);
    return address < candidate.Address + candidate.Bytes ? &candidate : nullptr;
}

std::optional<ProgramString> ProgramStrings::ReadAt(PeImage const& image, uint64 address, std::size_t minimumLength)
{
    uint64 const base = image.GetImageBase();
    if (address < base || address - base > std::numeric_limits<uint32>::max())
        return std::nullopt;
    uint32 const rva = static_cast<uint32>(address - base);
    PeSection const* const section = image.SectionOfRva(rva);
    if (section == nullptr || section->IsExecutable() || rva - section->VirtualAddress >= FileBackedSize(*section))
        return std::nullopt;
    uint32 const available = FileBackedSize(*section) - (rva - section->VirtualAddress);
    std::span<uint8 const> const bytes = image.ReadRva(rva, static_cast<uint32>(std::min<std::size_t>(available, 2 * MaximumLength + 2)));

    std::size_t length = 0;
    while (length < bytes.size() && length < MaximumLength && IsPrintable(bytes[length]))
        ++length;
    if (length >= minimumLength && length < bytes.size() && bytes[length] == 0)
        return ProgramString{ address, static_cast<uint32>(length), false, std::string(reinterpret_cast<char const*>(bytes.data()), length) };

    std::string text;
    std::size_t const units = bytes.size() / 2;
    std::size_t unit = 0;
    while (unit < units && unit < MaximumLength && IsPrintable(UnitAt(bytes, unit)))
        text.push_back(static_cast<char>(UnitAt(bytes, unit++)));
    if (unit >= minimumLength && unit < units && UnitAt(bytes, unit) == 0)
        return ProgramString{ address, static_cast<uint32>(2 * unit), true, std::move(text) };
    return std::nullopt;
}

std::unordered_map<uint32, std::vector<std::string>> ProgramStrings::ClassNames() const
{
    std::unordered_map<uint32, std::vector<std::string>> names;
    for (ProgramString const& string : _strings)
    {
        if (string.Text.empty())
            continue;
        for (std::string_view const prefix : { std::string_view(), std::string_view("class "), std::string_view("struct ") })
        {
            std::string name = std::string(prefix) + string.Text;
            std::vector<std::string>& bucket = names[StringHash::KiStringHash(name)];
            if (std::find(bucket.begin(), bucket.end(), name) == bucket.end())
                bucket.push_back(std::move(name));
        }
    }
    return names;
}
