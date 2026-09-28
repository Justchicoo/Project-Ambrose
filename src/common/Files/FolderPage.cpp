/*
 * Project Ambrose by Imjustchico
 * Filters by a name holding the filter text, ignoring ASCII case, counts what is left, sorts folders first and then by the chosen key with the name as a tie-break, where kind means the extension, and cuts the page from the offset, clamping the limit so a caller can never ask for more than a thousand rows at once.
 */

#include "FolderPage.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace
{
    std::string Folded(std::string_view text)
    {
        std::string folded(text);
        for (char& c : folded)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        return folded;
    }

    std::string Extension(std::string_view name)
    {
        std::size_t const dot = name.rfind('.');
        return dot == std::string_view::npos || dot == 0 ? std::string() : Folded(name.substr(dot + 1));
    }

    int Compare(Ambrose::JailListed const& left, Ambrose::JailListed const& right, Ambrose::FolderSort sort)
    {
        switch (sort)
        {
            case Ambrose::FolderSort::Size:
                if (left.Size != right.Size)
                    return left.Size < right.Size ? -1 : 1;
                break;
            case Ambrose::FolderSort::Modified:
                if (left.ModifiedEpochMs != right.ModifiedEpochMs)
                    return left.ModifiedEpochMs < right.ModifiedEpochMs ? -1 : 1;
                break;
            case Ambrose::FolderSort::Kind:
            {
                int const kind = Extension(left.Name).compare(Extension(right.Name));
                if (kind != 0)
                    return kind < 0 ? -1 : 1;
                break;
            }
            case Ambrose::FolderSort::Name:
                break;
        }
        int const folded = Folded(left.Name).compare(Folded(right.Name));
        if (folded != 0)
            return folded < 0 ? -1 : 1;
        int const exact = left.Name.compare(right.Name);
        return exact < 0 ? -1 : exact > 0 ? 1 : 0;
    }
}

std::optional<Ambrose::FolderSort> Ambrose::FolderPage::ParseSort(std::string_view text) noexcept
{
    if (text.empty() || text == "name")
        return FolderSort::Name;
    if (text == "size")
        return FolderSort::Size;
    if (text == "modified")
        return FolderSort::Modified;
    if (text == "kind")
        return FolderSort::Kind;
    return std::nullopt;
}

std::string_view Ambrose::FolderPage::SortName(FolderSort sort) noexcept
{
    switch (sort)
    {
        case FolderSort::Size: return "size";
        case FolderSort::Modified: return "modified";
        case FolderSort::Kind: return "kind";
        case FolderSort::Name: break;
    }
    return "name";
}

bool Ambrose::FolderPage::Matches(std::string_view name, std::string_view filter)
{
    if (filter.empty())
        return true;
    return Folded(name).find(Folded(filter)) != std::string::npos;
}

Ambrose::FolderPageResult Ambrose::FolderPage::Build(std::vector<JailListed> entries, FolderQuery const& query, bool truncated)
{
    std::erase_if(entries, [&query](JailListed const& entry) { return !Matches(entry.Name, query.Filter); });
    std::sort(entries.begin(), entries.end(), [&query](JailListed const& left, JailListed const& right)
    {
        bool const leftFolder = left.Kind == EntryKind::Folder;
        bool const rightFolder = right.Kind == EntryKind::Folder;
        if (leftFolder != rightFolder)
            return leftFolder;
        int const order = Compare(left, right, query.Sort);
        return query.Descending ? order > 0 : order < 0;
    });
    FolderPageResult page;
    page.Total = entries.size();
    page.Limit = std::clamp<std::size_t>(query.Limit, 1, MaxLimit);
    page.Offset = std::min(query.Offset, entries.size());
    page.Truncated = truncated;
    std::size_t const end = std::min(entries.size(), page.Offset + page.Limit);
    page.Entries.assign(std::make_move_iterator(entries.begin() + static_cast<std::ptrdiff_t>(page.Offset)),
        std::make_move_iterator(entries.begin() + static_cast<std::ptrdiff_t>(end)));
    return page;
}
