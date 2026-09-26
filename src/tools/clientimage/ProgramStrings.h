/*
 * Project Ambrose by Imjustchico
 * The text a client program carries: every run of at least MinimumLength printable characters, ASCII or UTF-16LE, that a terminator ends inside a section holding no code, found by the text it holds whatever its case, with the address it starts at, and the string a given address falls inside; ReadAt reads the text from an address to its terminator, which is how code that reads a string, often a suffix the linker shares with a longer one, is shown with the words it reads.
 */

#ifndef AMBROSE_PROGRAMSTRINGS_H
#define AMBROSE_PROGRAMSTRINGS_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PeImage;

struct ProgramString
{
    uint64 Address = 0;
    uint32 Bytes = 0;
    bool Wide = false;
    std::string Text;
};

class ProgramStrings
{
public:
    static constexpr std::size_t MinimumLength = 4;
    static constexpr std::size_t MaximumLength = 4096;

    explicit ProgramStrings(PeImage const& image);

    std::vector<ProgramString const*> Find(std::string_view text) const;
    ProgramString const* Containing(uint64 address) const;
    std::size_t Size() const noexcept { return _strings.size(); }

    static std::optional<ProgramString> ReadAt(PeImage const& image, uint64 address, std::size_t minimumLength);

private:
    std::vector<ProgramString> _strings;
};

#endif
