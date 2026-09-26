/*
 * Project Ambrose by Imjustchico
 * Maps every import slot to the import it holds once, and describes a target by the first of what it is: an import, a name it was given, or a string of at least ShortestString characters; a call or jump names its branch target and anything else the address its RIP-relative operand reads.
 */

#include "CodeAnnotator.h"
#include "CodeIndex.h"
#include "PeImage.h"
#include "ProgramStrings.h"

#include <fmt/format.h>

#include <utility>

CodeAnnotator::CodeAnnotator(PeImage const& image, std::unordered_map<uint64, std::string> names) : _image(image), _names(std::move(names))
{
    for (PeImport const& import : image.GetImports())
        _imports.emplace(image.GetImageBase() + import.SlotRva, import.Ordinal ? fmt::format("{}!#{}", import.Dll, *import.Ordinal) : fmt::format("{}!{}", import.Dll, import.Name));
}

std::string CodeAnnotator::Describe(DecodedInstruction const& instruction) const
{
    if (instruction.RipRelativeTarget)
        if (std::string described = Describe(*instruction.RipRelativeTarget); !described.empty())
            return described;
    if (instruction.BranchTarget)
        return NameOf(*instruction.BranchTarget);
    return {};
}

std::string CodeAnnotator::Describe(uint64 target) const
{
    if (auto const import = _imports.find(target); import != _imports.end())
        return import->second;
    if (std::string name = NameOf(target); !name.empty())
        return name;
    if (std::optional<ProgramString> const string = ProgramStrings::ReadAt(_image, target, ShortestString))
        return Quote(*string);
    return {};
}

std::string CodeAnnotator::NameOf(uint64 address) const
{
    auto const found = _names.find(address);
    return found == _names.end() ? std::string() : found->second;
}

std::string CodeAnnotator::Quote(ProgramString const& string, std::size_t limit)
{
    std::string quoted = string.Wide ? "L\"" : "\"";
    std::size_t written = 0;
    for (char const c : string.Text)
    {
        if (written == limit)
        {
            quoted += "...";
            break;
        }
        switch (c)
        {
            case '\\':
                quoted += "\\\\";
                break;
            case '"':
                quoted += "\\\"";
                break;
            case '\t':
                quoted += "\\t";
                break;
            case '\n':
                quoted += "\\n";
                break;
            case '\r':
                quoted += "\\r";
                break;
            default:
                quoted += c;
                break;
        }
        ++written;
    }
    quoted += '"';
    return quoted;
}
