/*
 * Project Ambrose by Imjustchico
 * The tiered spell group each tiered spell belongs to, read from TieredSpellsGroupInfo.xml in the install's Root.wad, where the client finds the group index it keeps in a spellbook's tracker for a tiered spell by the spell's exact name; a spell the file does not name belongs to no group.
 */

#ifndef AMBROSE_TIEREDSPELLGROUPS_H
#define AMBROSE_TIEREDSPELLGROUPS_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class KiwadArchive;

class TieredSpellGroups
{
public:
    static constexpr std::string_view Entry = "TieredSpellsGroupInfo.xml";

    static std::optional<TieredSpellGroups> Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors);

    std::optional<int32> Find(std::string_view spellName) const;
    std::size_t Size() const noexcept { return _groups.size(); }
    std::size_t CountGroups() const noexcept { return _groupCount; }

private:
    std::unordered_map<std::string, int32> _groups;
    std::size_t _groupCount = 0;
};

#endif
