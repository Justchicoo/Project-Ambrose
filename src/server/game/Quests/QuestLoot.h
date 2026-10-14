/*
 * Project Ambrose by Imjustchico
 * Turns the wiki's quest rewards into loot tables for the quests a CSR package imports: each quest the wiki rewards gets one table of the gold, experience and items the package's own results do not already give, an item given to one school only gated on a school requirement list, and a ResLoot quest result that rolls the table, and writes them marked with the package as their source, replacing what an earlier import of it wrote.
 */

#ifndef AMBROSE_QUESTLOOT_H
#define AMBROSE_QUESTLOOT_H

#include "QuestWikiData.h"
#include "TypeRegistry.h"
#include "Types.h"
#include "WorldSqlScript.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct QuestLootEntry
{
    std::string Table;
    uint32 Position = 0;
    std::string Type;
    uint32 TemplateId = 0;
    uint32 Count = 1;
    std::string SchoolList;
};

struct QuestLootRows
{
    std::vector<std::string> Tables;
    std::vector<QuestLootEntry> Entries;
    std::vector<std::string> Schools;
    std::size_t GoldWritten = 0;
    std::size_t GoldFromPackage = 0;
    std::size_t ExperienceWritten = 0;
    std::size_t ExperienceFromPackage = 0;
    std::size_t ItemsWritten = 0;
    std::size_t ItemsFromPackage = 0;
    std::size_t ResultsNotEncoded = 0;
};

class QuestLoot
{
public:
    static constexpr std::string_view ResultClass = "class ResLoot";

    QuestLoot() = delete;

    static std::string TableOf(std::string_view source, std::string_view quest);
    static std::string SchoolListOf(std::string_view source, std::string_view school);
    static std::string LootTypeOf(std::string_view rewardType);
    static QuestLootRows Apply(TypeCatalogPtr const& catalog, std::vector<QuestWikiReward> const& rewards, std::vector<QuestWikiRewardItem> const& items, QuestPackageRead& read,
        std::string const& source);
    static WorldSqlScript BuildScript(QuestLootRows const& loot, std::string const& source);
};

#endif
