/*
 * Project Ambrose by Imjustchico
 * One page of a folder's listing, worked out with no disk in sight: entries are filtered by name first so the total a page reports is the size of the filtered set, folders come before everything else, then the chosen order by name, size, modification time or kind with the name breaking ties, then the offset and a limit of 1 to 1000, and the page says when the folder held more entries than were read.
 */

#ifndef AMBROSE_FOLDERPAGE_H
#define AMBROSE_FOLDERPAGE_H

#include "FileJail.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Ambrose
{
    enum class FolderSort : uint8
    {
        Name,
        Size,
        Modified,
        Kind
    };

    struct FolderQuery
    {
        std::string Filter = {};
        FolderSort Sort = FolderSort::Name;
        bool Descending = false;
        std::size_t Offset = 0;
        std::size_t Limit = 100;
    };

    struct FolderPageResult
    {
        std::vector<JailListed> Entries = {};
        std::size_t Total = 0;
        std::size_t Offset = 0;
        std::size_t Limit = 0;
        bool Truncated = false;
    };

    namespace FolderPage
    {
        inline constexpr std::size_t MaxLimit = 1000;
        inline constexpr std::size_t DefaultLimit = 100;

        std::optional<FolderSort> ParseSort(std::string_view text) noexcept;
        std::string_view SortName(FolderSort sort) noexcept;
        bool Matches(std::string_view name, std::string_view filter);
        FolderPageResult Build(std::vector<JailListed> entries, FolderQuery const& query, bool truncated);
    }
}

#endif
