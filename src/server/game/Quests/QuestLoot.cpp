/*
 * Project Ambrose by Imjustchico
 * Builds each rewarded quest's loot table from the wiki's gold, experience and matched items, leaving out a kind the quest's own results already give, adds the ResLoot result after the quest's other results, and writes the tables, entries and school requirement lists of one package.
 */

#include "QuestLoot.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace
{
    bool Gives(QuestPackageQuest const& quest, std::initializer_list<std::string_view> classes)
    {
        return std::any_of(quest.Rows.Results.begin(), quest.Rows.Results.end(), [classes](QuestResultRow const& row)
        {
            return std::find(classes.begin(), classes.end(), row.Class) != classes.end();
        });
    }

    std::string SchoolName(std::string_view school)
    {
        std::string name(school);
        if (!name.empty())
            name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
        return name;
    }
}

std::string QuestLoot::TableOf(std::string_view source, std::string_view quest)
{
    return fmt::format("{}:quest:{}", source, quest);
}

std::string QuestLoot::SchoolListOf(std::string_view source, std::string_view school)
{
    return fmt::format("{}:school:{}", source, SchoolName(school));
}

std::string QuestLoot::LootTypeOf(std::string_view rewardType)
{
    if (rewardType == "Spell" || rewardType == "Spellement")
        return "SPELL";
    if (rewardType == "Treasure Card")
        return "TREASURE_CARD";
    if (rewardType == "Reagent")
        return "REAGENT";
    return "ITEM";
}

QuestLootRows QuestLoot::Apply(TypeCatalogPtr const& catalog, std::vector<QuestWikiReward> const& rewards, std::vector<QuestWikiRewardItem> const& items, QuestPackageRead& read,
    std::string const& source)
{
    std::map<std::string, std::vector<QuestLootEntry>, std::less<>> tables;
    std::set<std::string, std::less<>> schools;
    std::map<std::string, QuestPackageQuest*, std::less<>> quests;
    for (QuestPackageQuest& quest : read.Quests)
        if (quest.IsClean() && !quest.Rows.Quests.empty())
            quests.emplace(quest.Name, &quest);

    QuestLootRows loot;
    for (QuestWikiReward const& reward : rewards)
    {
        auto const found = quests.find(reward.Quest);
        if (found == quests.end())
            continue;
        std::vector<QuestLootEntry>& entries = tables[reward.Quest];
        std::string const table = TableOf(source, reward.Quest);
        if (reward.Gold != 0)
        {
            if (Gives(*found->second, { "class ResAddGold" }))
                ++loot.GoldFromPackage;
            else
            {
                entries.push_back(QuestLootEntry{ table, 0, "GOLD", 0, reward.Gold, {} });
                ++loot.GoldWritten;
            }
        }
        if (reward.Experience != 0)
        {
            if (Gives(*found->second, { "class ResAddMagicXP" }))
                ++loot.ExperienceFromPackage;
            else
            {
                entries.push_back(QuestLootEntry{ table, 0, "XP", 0, reward.Experience, {} });
                ++loot.ExperienceWritten;
            }
        }
    }
    for (QuestWikiRewardItem const& item : items)
    {
        auto const found = quests.find(item.Quest);
        if (found == quests.end())
            continue;
        if (Gives(*found->second, { "class ResItemLoot", "class ResAddSpell", "class ResAddTreasureSpell" }))
        {
            ++loot.ItemsFromPackage;
            continue;
        }
        std::string list;
        if (!item.School.empty())
        {
            list = SchoolListOf(source, item.School);
            schools.insert(SchoolName(item.School));
        }
        tables[item.Quest].push_back(QuestLootEntry{ TableOf(source, item.Quest), 0, LootTypeOf(item.Type), item.TemplateId, item.Count, std::move(list) });
        ++loot.ItemsWritten;
    }

    for (auto& [name, entries] : tables)
    {
        if (entries.empty())
            continue;
        PropertyObjectPtr result = PropertyObject::Create(catalog, ResultClass);
        std::string const table = TableOf(source, name);
        EncodeResult encoded = result && result->Set("m_lootTable", table) == PropertySetResult::Ok ? ObjectSerializer::Encode(result.get(), ZoneExtractor::DataOptions()) : EncodeResult{};
        if (!result || !encoded.Ok())
        {
            ++loot.ResultsNotEncoded;
            continue;
        }
        QuestPackageQuest& quest = *quests.at(name);
        uint32 position = 0;
        for (QuestResultRow const& row : quest.Rows.Results)
            if (row.Goal.empty())
                position = std::max(position, row.Position + 1);
        quest.Rows.Results.push_back(QuestResultRow{ name, {}, position, std::string(ResultClass), std::move(encoded.Bytes), false });
        loot.Tables.push_back(table);
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            entries[index].Position = static_cast<uint32>(index);
            loot.Entries.push_back(std::move(entries[index]));
        }
    }
    loot.Schools.assign(schools.begin(), schools.end());
    return loot;
}

WorldSqlScript QuestLoot::BuildScript(QuestLootRows const& loot, std::string const& source)
{
    using Value = WorldSqlScript::Value;
    auto const text = [](std::string const& value) { return value.empty() ? Value{ std::monostate{} } : Value{ value }; };
    std::vector<WorldSqlScript::Row> tables;
    for (std::string const& table : loot.Tables)
        tables.push_back({ table, source });
    std::vector<WorldSqlScript::Row> lists;
    std::vector<WorldSqlScript::Row> requirements;
    for (std::string const& school : loot.Schools)
    {
        std::string const list = SchoolListOf(source, school);
        lists.push_back({ list, std::string("AND"), uint64{ 0 }, source });
        requirements.push_back({ list, uint64{ 0 }, std::string("ReqSchoolOfFocus"), uint64{ 0 }, school });
    }
    std::vector<WorldSqlScript::Row> entries;
    for (QuestLootEntry const& entry : loot.Entries)
        entries.push_back({ entry.Table, uint64{ entry.Position }, entry.Type, entry.TemplateId ? Value{ uint64{ entry.TemplateId } } : Value{ std::monostate{} },
            uint64{ entry.Count }, uint64{ entry.Count }, 1.0, uint64{ 0 }, text(entry.SchoolList) });

    WorldSqlScript script;
    script.ReplaceRows("loot_table", "source", Value{ source }, { "id", "source" }, tables);
    script.ReplaceRows("requirement_list", "source", Value{ source }, { "id", "operator", "apply_not", "source" }, lists);
    script.InsertRows("requirement", { "list_id", "position", "type", "apply_not", "magic_school" }, requirements);
    script.InsertRows("loot_entry", { "table_id", "position", "type", "template_id", "min_count", "max_count", "chance", "group_id", "requirement_list_id" }, entries);
    return script;
}
