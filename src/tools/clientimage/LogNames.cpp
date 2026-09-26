/*
 * Project Ambrose by Imjustchico
 * Reads every string a RIP-relative operand names once, keeps the source paths, which end in a C or C++ source or header extension after a folder, and the qualified names, a scope of identifiers and template arguments, which alone may hold spaces, commas and pointer and reference marks, then :: and an identifier, a destructor or an operator, then pairs each function's name reads with its path reads by distance.
 */

#include "LogNames.h"
#include "CodeIndex.h"
#include "PeImage.h"
#include "ProgramStrings.h"
#include "StringUtil.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>

namespace
{
    constexpr std::size_t ShortestName = 4;
    constexpr std::size_t LongestName = 256;
    constexpr std::size_t MaxOperatorLength = 12;
    constexpr std::string_view SourceExtensions[] = { ".cpp", ".cxx", ".cc", ".c", ".hpp", ".h", ".inl" };

    struct Read
    {
        uint64 Site = 0;
        std::optional<std::size_t> Name;
    };

    bool IsIdentifierStart(char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }

    bool IsIdentifierCharacter(char c) noexcept
    {
        return IsIdentifierStart(c) || (c >= '0' && c <= '9');
    }

    bool IsScope(std::string_view scope, bool opensWithIdentifier) noexcept
    {
        int depth = 0;
        for (char const c : scope)
        {
            if (c == '<')
                ++depth;
            else if (c == '>')
            {
                if (--depth < 0)
                    return false;
            }
            else if (!IsIdentifierCharacter(c) && c != ':' && (depth == 0 || (c != ' ' && c != ',' && c != '&' && c != '*' && c != '(' && c != ')')))
                return false;
        }
        return depth == 0 && !scope.empty() && (!opensWithIdentifier || IsIdentifierStart(scope.front()));
    }

    bool IsMember(std::string_view member) noexcept
    {
        if (member.starts_with("operator"))
        {
            std::string_view const symbol = Ambrose::Trim(member.substr(8));
            return !symbol.empty() && symbol.size() <= MaxOperatorLength
                && std::all_of(symbol.begin(), symbol.end(), [](char c) { return c != '%' && c != '"' && c != '\\' && c >= 0x20 && c <= 0x7E; });
        }
        if (member.starts_with('~'))
            member.remove_prefix(1);
        std::size_t const angle = member.find('<');
        std::string_view const identifier = member.substr(0, angle);
        if (identifier.empty() || !IsIdentifierStart(identifier.front()) || !std::all_of(identifier.begin(), identifier.end(), IsIdentifierCharacter))
            return false;
        return angle == std::string_view::npos || IsScope(member.substr(angle), false);
    }
}

bool LogNames::IsQualifiedName(std::string_view text)
{
    if (text.size() < ShortestName || text.size() > LongestName || !IsIdentifierStart(text.front()))
        return false;
    std::size_t split = std::string_view::npos;
    int depth = 0;
    for (std::size_t index = 0; index + 1 < text.size(); ++index)
    {
        if (text[index] == '<')
            ++depth;
        else if (text[index] == '>')
            --depth;
        else if (depth == 0 && text[index] == ':' && text[index + 1] == ':')
        {
            split = index;
            ++index;
        }
    }
    if (split == std::string_view::npos || split == 0)
        return false;
    return IsScope(text.substr(0, split), true) && IsMember(text.substr(split + 2));
}

bool LogNames::IsSourcePath(std::string_view text)
{
    if (text.find('\\') == std::string_view::npos && text.find('/') == std::string_view::npos)
        return false;
    return std::any_of(std::begin(SourceExtensions), std::end(SourceExtensions), [text](std::string_view extension)
    {
        return text.size() > extension.size() && Ambrose::EqualsIgnoreCase(text.substr(text.size() - extension.size()), extension);
    });
}

std::vector<FunctionLogNames> LogNames::Find(PeImage const& image, CodeIndex const& code)
{
    std::vector<std::string> names;
    std::map<uint64, std::vector<Read>> reads;
    for (uint64 const target : code.RipTargets())
    {
        std::optional<ProgramString> const string = ProgramStrings::ReadAt(image, target, ShortestName);
        if (!string || string->Wide)
            continue;
        bool const path = IsSourcePath(string->Text);
        if (!path && !IsQualifiedName(string->Text))
            continue;
        std::optional<std::size_t> name;
        if (!path)
        {
            names.push_back(string->Text);
            name = names.size() - 1;
        }
        for (uint64 const site : code.RipReferences(target))
            if (std::optional<uint64> const function = code.FunctionStart(site))
                reads[*function].push_back({ site, name });
    }

    std::vector<FunctionLogNames> found;
    for (auto& [function, functionReads] : reads)
    {
        std::sort(functionReads.begin(), functionReads.end(), [](Read const& left, Read const& right) { return left.Site < right.Site; });
        FunctionLogNames logged{ function, {} };
        for (Read const& read : functionReads)
        {
            if (!read.Name)
                continue;
            bool const besidePath = std::any_of(functionReads.begin(), functionReads.end(), [&read](Read const& other)
            {
                return !other.Name && (other.Site > read.Site ? other.Site - read.Site : read.Site - other.Site) <= NeighborBytes;
            });
            std::string const& name = names[*read.Name];
            if (besidePath && std::find(logged.Names.begin(), logged.Names.end(), name) == logged.Names.end())
                logged.Names.push_back(name);
        }
        if (!logged.Names.empty())
            found.push_back(std::move(logged));
    }
    return found;
}
