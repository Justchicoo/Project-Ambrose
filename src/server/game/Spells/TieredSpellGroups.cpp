/*
 * Project Ambrose by Imjustchico
 * Reads the group info list through its typed views and refuses a file that does not decode, an entry that is not group info or carries no group, and a spell named twice, since which of two groups the client would find for it cannot be known.
 */

#include "TieredSpellGroups.h"
#include "BindFile.h"
#include "KiwadArchive.h"
#include "ObjectViews.h"

#include <fmt/format.h>

#include <set>
#include <utility>

std::optional<TieredSpellGroups> TieredSpellGroups::Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors)
{
    KiwadReadResult const bytes = root.Read(Entry);
    if (!bytes.Succeeded())
    {
        errors.push_back(fmt::format("{} cannot be read: {}", Entry, bytes.Error));
        return std::nullopt;
    }
    BindReadResult const decoded = BindFile::Read(catalog, bytes.Data);
    std::optional<TieredSpellGroupInfoListView> const list = decoded.Ok() && decoded.Decoded.Object ? TieredSpellGroupInfoListView::From(*decoded.Decoded.Object) : std::nullopt;
    if (!list)
    {
        errors.push_back(fmt::format("{} does not read as a TieredSpellGroupInfoList: {}", Entry, decoded.Ok() ? std::string("its root is another class") : decoded.Detail));
        return std::nullopt;
    }
    TieredSpellGroups groups;
    std::set<int32> indices;
    std::size_t position = 0;
    std::size_t const failures = errors.size();
    for (PropertyValue const& entry : list->GetGroups())
    {
        std::optional<TieredSpellGroupInfoView> const info = entry.AsObject() ? TieredSpellGroupInfoView::From(*entry.AsObject()) : std::nullopt;
        std::optional<TieredSpellGroupInfoDataView> const data = info && info->GetData() ? TieredSpellGroupInfoDataView::From(*info->GetData()) : std::nullopt;
        if (!info || !data)
        {
            errors.push_back(fmt::format("{} entry {} {}", Entry, position, info ? fmt::format("names {} but carries no group", info->GetSpellName()) : std::string("is not TieredSpellGroupInfo")));
            ++position;
            continue;
        }
        auto const [known, added] = groups._groups.emplace(info->GetSpellName(), data->GetGroupIndex());
        if (!added)
            errors.push_back(fmt::format("{} names {} twice, in groups {} and {}", Entry, info->GetSpellName(), known->second, data->GetGroupIndex()));
        indices.insert(data->GetGroupIndex());
        ++position;
    }
    if (errors.size() != failures)
        return std::nullopt;
    groups._groupCount = indices.size();
    return groups;
}

std::optional<int32> TieredSpellGroups::Find(std::string_view spellName) const
{
    auto const found = _groups.find(std::string(spellName));
    if (found == _groups.end())
        return std::nullopt;
    return found->second;
}
