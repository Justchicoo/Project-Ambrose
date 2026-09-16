/*
 * Project Ambrose by Imjustchico
 * Parses one client locale .lang file of at most 64 MiB: UTF-16LE text with a byte order mark, a first line of 1:<Stem>, then for each entry a non-empty key line, a metadata line that is usually blank and a text line, with blank padding allowed after the last entry, every line converted to UTF-8, a key made of digits kept as the string it is, and the full key formed as <Stem>_<Key>.
 */

#ifndef AMBROSE_LANGFILE_H
#define AMBROSE_LANGFILE_H

#include "Types.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct LangEntry
{
    std::string Key;
    std::string Metadata;
    std::string Text;
};

struct LangParseResult
{
    std::string Stem;
    std::vector<LangEntry> Entries;
    std::string Error;

    bool Ok() const noexcept { return Error.empty(); }
};

class LangFile
{
public:
    static constexpr std::string_view HeaderPrefix = "1:";
    static constexpr std::size_t MaxFileBytes = std::size_t{ 64 } << 20;

    LangFile() = delete;

    static LangParseResult Parse(std::span<uint8 const> bytes, std::size_t maxBytes = MaxFileBytes);
    static std::string MakeKey(std::string_view stem, std::string_view key);
};

#endif
