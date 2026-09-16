/*
 * Project Ambrose by Imjustchico
 * Checks the size limit, byte order mark and even length, converts the UTF-16LE text to UTF-8 and refuses unpaired surrogates, splits it into lines at CRLF or a lone LF, drops blank padding lines after the last entry and gives a last entry whose text line was cut by the final line break an empty text, reads the stem from the header, and takes the rest three lines at a time, refusing an empty key or an unfinished entry and naming the line each problem is on.
 */

#include "LangFile.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace
{
    constexpr std::size_t BomSize = 2;
    constexpr std::size_t LinesPerEntry = 3;

    LangParseResult Refuse(std::string error)
    {
        LangParseResult result;
        result.Error = std::move(error);
        return result;
    }
}

LangParseResult LangFile::Parse(std::span<uint8 const> bytes, std::size_t maxBytes)
{
    if (bytes.size() > maxBytes)
        return Refuse(fmt::format("is {} bytes, more than the {} a .lang file may hold", bytes.size(), maxBytes));
    if (bytes.size() < BomSize || bytes[0] != 0xFF || bytes[1] != 0xFE)
        return Refuse("does not start with a UTF-16LE byte order mark");
    if (bytes.size() % 2 != 0)
        return Refuse(fmt::format("is {} bytes, which cannot be whole UTF-16 units", bytes.size()));
    std::optional<std::string> const text = [&bytes]() -> std::optional<std::string>
    {
        std::optional<std::u16string> const wide = Utf::Utf16LEBytesToString(bytes.subspan(BomSize), Utf::InvalidPolicy::Reject);
        return wide ? Utf::Utf16ToUtf8(*wide, Utf::InvalidPolicy::Reject) : std::nullopt;
    }();
    if (!text)
        return Refuse("holds UTF-16 text with an unpaired surrogate");

    std::vector<std::string_view> lines;
    std::string_view remaining = *text;
    while (!remaining.empty())
    {
        std::size_t const end = remaining.find('\n');
        std::string_view line = remaining.substr(0, end);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        lines.push_back(line);
        if (end == std::string_view::npos)
            break;
        remaining.remove_prefix(end + 1);
    }
    if (lines.empty() || !lines.front().starts_with(HeaderPrefix) || lines.front().size() == HeaderPrefix.size())
        return Refuse(fmt::format("does not start with a {}<Stem> header line", HeaderPrefix));

    std::size_t const partial = (lines.size() - 1) % LinesPerEntry;
    if (partial != 0 && std::all_of(lines.end() - static_cast<std::ptrdiff_t>(partial), lines.end(), [](std::string_view line) { return line.empty(); }))
        lines.resize(lines.size() - partial);
    else if (partial == LinesPerEntry - 1 && text->back() == '\n')
        lines.emplace_back();
    if ((lines.size() - 1) % LinesPerEntry != 0)
        return Refuse(fmt::format("ends on line {} in the middle of an entry, which needs a key, a metadata and a text line", lines.size()));

    LangParseResult result;
    result.Stem = std::string(lines.front().substr(HeaderPrefix.size()));
    result.Entries.reserve((lines.size() - 1) / LinesPerEntry);
    for (std::size_t line = 1; line < lines.size(); line += LinesPerEntry)
    {
        if (lines[line].empty())
            return Refuse(fmt::format("holds an empty key on line {}", line + 1));
        result.Entries.push_back(LangEntry{ std::string(lines[line]), std::string(lines[line + 1]), std::string(lines[line + 2]) });
    }
    return result;
}

std::string LangFile::MakeKey(std::string_view stem, std::string_view key)
{
    std::string full;
    full.reserve(stem.size() + 1 + key.size());
    full.append(stem);
    full.push_back('_');
    full.append(key);
    return full;
}
