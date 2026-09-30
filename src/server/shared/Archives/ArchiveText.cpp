/*
 * Project Ambrose by Imjustchico
 * Reads each entry named .xml, .txt or .notxml that is not a BINd file and gathers every run that opens with m_ at the start of a word and goes on with letters, digits and underscores, and every class it names, as an element written class.Name or a quoted "class Name" or "struct Name", each sorted and once.
 */

#include "ArchiveText.h"
#include "KiwadArchive.h"
#include "StringUtil.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <string_view>

namespace
{
    bool IsText(std::string_view name)
    {
        std::string const lower = Ambrose::ToLower(name);
        return lower.ends_with(".xml") || lower.ends_with(".txt") || lower.ends_with(".notxml");
    }

    bool InName(unsigned char c)
    {
        return std::isalnum(c) != 0 || c == '_';
    }

    template<typename Visit>
    void ForEachText(KiwadArchive const& archive, Visit&& visit)
    {
        for (KiwadEntry const& entry : archive.GetEntries())
        {
            if (!IsText(entry.Name))
                continue;
            KiwadReadResult const read = archive.Read(entry);
            if (!read.Succeeded() || (read.Data.size() >= 4 && std::equal(read.Data.begin(), read.Data.begin() + 4, "BINd")))
                continue;
            visit(std::string_view(reinterpret_cast<char const*>(read.Data.data()), read.Data.size()));
        }
    }

    std::size_t NameEnd(std::string_view text, std::size_t at)
    {
        while (at < text.size() && (InName(static_cast<unsigned char>(text[at])) || (text[at] == ':' && at + 1 < text.size() && text[at + 1] == ':')))
            at += text[at] == ':' ? 2 : 1;
        return at;
    }
}

std::vector<std::string> ArchiveText::PropertyNames(KiwadArchive const& archive)
{
    std::set<std::string> found;
    ForEachText(archive, [&found](std::string_view text)
    {
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
    });
    return { found.begin(), found.end() };
}

std::vector<std::string> ArchiveText::ClassNames(KiwadArchive const& archive)
{
    struct Form
    {
        std::string_view Opening;
        std::string_view Keyword;
        std::string_view Closings;
    };
    static constexpr std::array<Form, 3> Forms{ { { "<class.", "class ", "> /" }, { "\"class ", "class ", "\"" }, { "\"struct ", "struct ", "\"" } } };
    std::set<std::string> found;
    ForEachText(archive, [&found](std::string_view text)
    {
        for (Form const& form : Forms)
            for (std::size_t at = text.find(form.Opening); at != std::string_view::npos; at = text.find(form.Opening, at + form.Opening.size()))
            {
                std::size_t const start = at + form.Opening.size();
                std::size_t const end = NameEnd(text, start);
                if (end > start && end < text.size() && form.Closings.find(text[end]) != std::string_view::npos && !std::isdigit(static_cast<unsigned char>(text[start])))
                    found.emplace(std::string(form.Keyword).append(text.substr(start, end - start)));
            }
    });
    return { found.begin(), found.end() };
}
