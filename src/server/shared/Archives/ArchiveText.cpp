/*
 * Project Ambrose by Imjustchico
 * Reads each entry named .xml or .txt that is not a BINd file and gathers every run that opens with m_ at the start of a word and goes on with letters, digits and underscores, sorted and each once.
 */

#include "ArchiveText.h"
#include "KiwadArchive.h"
#include "StringUtil.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>

namespace
{
    bool IsText(std::string_view name)
    {
        std::string const lower = Ambrose::ToLower(name);
        return lower.ends_with(".xml") || lower.ends_with(".txt");
    }

    bool InName(unsigned char c)
    {
        return std::isalnum(c) != 0 || c == '_';
    }
}

std::vector<std::string> ArchiveText::PropertyNames(KiwadArchive const& archive)
{
    std::set<std::string> found;
    for (KiwadEntry const& entry : archive.GetEntries())
    {
        if (!IsText(entry.Name))
            continue;
        KiwadReadResult const read = archive.Read(entry);
        if (!read.Succeeded() || (read.Data.size() >= 4 && std::equal(read.Data.begin(), read.Data.begin() + 4, "BINd")))
            continue;
        std::string_view const text(reinterpret_cast<char const*>(read.Data.data()), read.Data.size());
        for (std::size_t at = text.find("m_"); at != std::string_view::npos; at = text.find("m_", at + 2))
        {
            if (at > 0 && InName(static_cast<unsigned char>(text[at - 1])))
                continue;
            std::size_t end = at + 2;
            while (end < text.size() && InName(static_cast<unsigned char>(text[end])))
                ++end;
            if (end > at + 2)
                found.emplace(text.substr(at, end - at));
        }
    }
    return { found.begin(), found.end() };
}
