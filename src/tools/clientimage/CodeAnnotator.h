/*
 * Project Ambrose by Imjustchico
 * Says what an instruction's targets are in the words a person reading the client's code wants: the import a call through a slot reaches, as DLL!Name or DLL!#ordinal, the name given for a function or global it calls, loads or jumps to, such as a message handler the program registers, and the text of a string it reads, ASCII or wide, quoted and cut at a limit.
 */

#ifndef AMBROSE_CODEANNOTATOR_H
#define AMBROSE_CODEANNOTATOR_H

#include "Types.h"

#include <cstddef>
#include <string>
#include <unordered_map>

class PeImage;
struct DecodedInstruction;
struct ProgramString;

class CodeAnnotator
{
public:
    static constexpr std::size_t QuotedLimit = 120;
    static constexpr std::size_t ShortestString = 2;

    CodeAnnotator(PeImage const& image, std::unordered_map<uint64, std::string> names);

    std::string Describe(DecodedInstruction const& instruction) const;
    std::string Describe(uint64 target) const;
    std::string NameOf(uint64 address) const;

    static std::string Quote(ProgramString const& string, std::size_t limit = QuotedLimit);

private:
    PeImage const& _image;
    std::unordered_map<uint64, std::string> _imports;
    std::unordered_map<uint64, std::string> _names;
};

#endif
