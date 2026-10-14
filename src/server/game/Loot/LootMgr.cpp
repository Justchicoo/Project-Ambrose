/*
 * Project Ambrose by Imjustchico
 * Reads loot_table, loot_entry and creature_loot from the world database, or from the row source a test gives it, refuses a generation with an entry or creature naming a table that is not there, a chance outside 0 to 1, a count range upside down or an item, spell, reagent or card entry with no template, keeping the serving one, and rolls a table for a wizard: an entry of chance 1 is always given, a chance below 1 is scaled by its kind's live Rate.Drop setting and capped at 1, group 0 entries roll on their own and each other group gives at most one entry by a single roll over its chances, and an entry whose requirement list the wizard fails is never given.
 */

#include "LootMgr.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "Random.h"
#include "ReloadMgr.h"
#include "RequirementMgr.h"
#include "Settings.h"

#include <fmt/format.h>

#include <algorithm>
#include <set>
#include <utility>

namespace
{
    constexpr char const* LootLog = "server.loot";

    constexpr std::pair<LootType, std::string_view> TypeNames[] = {
        { LootType::Gold, "GOLD" },
        { LootType::Xp, "XP" },
        { LootType::Item, "ITEM" },
        { LootType::Spell, "SPELL" },
        { LootType::Reagent, "REAGENT" },
        { LootType::TreasureCard, "TREASURE_CARD" },
    };

    std::optional<std::string_view> RateKey(LootType type)
    {
        switch (type)
        {
            case LootType::Item: return "Rate.Drop.Item";
            case LootType::Spell: return "Rate.Drop.Spell";
            case LootType::Reagent: return "Rate.Drop.Reagent";
            case LootType::TreasureCard: return "Rate.Drop.TreasureCard";
            default: return std::nullopt;
        }
    }

    double ScaledChance(LootEntryRow const& entry)
    {
        if (entry.Chance >= 1.0)
            return 1.0;
        std::optional<std::string_view> const key = RateKey(entry.Type);
        double const rate = key ? sSettings.Get<float>(*key) : 1.0;
        return std::min(entry.Chance * rate, 1.0);
    }

    template<typename Read>
    bool Select(std::string_view table, std::string_view sql, std::vector<std::string>& errors, Read read)
    {
        QueryResult result;
        if (!WorldDatabase.TryQuery(sql, result))
        {
            errors.push_back(std::string(table) + " could not be read");
            return false;
        }
        if (result)
        {
            do
                read(result->Fetch());
            while (result->NextRow());
        }
        return true;
    }

    bool NeedsTemplate(LootType type)
    {
        return type != LootType::Gold && type != LootType::Xp;
    }
}

LootMgr& LootMgr::Instance()
{
    static LootMgr instance;
    return instance;
}

std::optional<LootType> LootMgr::ParseType(std::string_view text)
{
    for (auto const& [type, name] : TypeNames)
        if (name == text)
            return type;
    return std::nullopt;
}

std::string_view LootMgr::TypeName(LootType type)
{
    for (auto const& [known, name] : TypeNames)
        if (known == type)
            return name;
    return "UNKNOWN";
}

void LootMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void LootMgr::SetRowSource(RowSource source)
{
    std::lock_guard const lock(_sourceMutex);
    _rowSource = std::move(source);
}

bool LootMgr::ReadWorldRows(LootRows& rows, std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so loot tables could not be read");
        return false;
    }

    std::size_t const before = errors.size();
    bool ok = true;
    ok &= Select("loot_table", "SELECT `id` FROM `loot_table` ORDER BY `id`", errors, [&rows](Field const* fields)
    {
        rows.Tables.push_back(fields[0].Get<std::string>());
    });
    ok &= Select("loot_entry", "SELECT `table_id`, `position`, `type`, `template_id`, `min_count`, `max_count`, `chance`, `group_id`, `requirement_list_id` FROM `loot_entry` "
        "ORDER BY `table_id`, `position`", errors, [&rows, &errors](Field const* fields)
    {
        LootEntryRow row;
        row.TableId = fields[0].Get<std::string>();
        row.Position = fields[1].Get<uint32>();
        std::string const type = fields[2].Get<std::string>();
        std::optional<LootType> const parsed = ParseType(type);
        if (!parsed)
        {
            errors.push_back(fmt::format("loot_entry {} {} has the unknown type {}", row.TableId, row.Position, type));
            return;
        }
        row.Type = *parsed;
        row.TemplateId = fields[3].IsNull() ? 0 : fields[3].Get<uint32>();
        row.MinCount = fields[4].Get<uint32>();
        row.MaxCount = fields[5].Get<uint32>();
        row.Chance = fields[6].Get<double>();
        row.Group = fields[7].Get<uint8>();
        if (!fields[8].IsNull())
            row.RequirementListId = fields[8].Get<std::string>();
        rows.Entries.push_back(std::move(row));
    });
    ok &= Select("creature_loot", "SELECT `template_id`, `table_id` FROM `creature_loot` ORDER BY `template_id`, `table_id`", errors, [&rows](Field const* fields)
    {
        rows.Creatures.push_back({ fields[0].Get<uint32>(), fields[1].Get<std::string>() });
    });
    return ok && errors.size() == before;
}

std::optional<LootStore> LootMgr::BuildStore(LootRows const& rows, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    LootStore store;
    for (std::string const& table : rows.Tables)
    {
        if (table.empty())
            errors.push_back("loot_table has an empty id");
        else if (!store.Tables.try_emplace(table).second)
            errors.push_back("loot_table duplicates id " + table);
    }
    std::set<std::pair<std::string, uint32>> positions;
    for (LootEntryRow const& entry : rows.Entries)
    {
        std::string const where = fmt::format("loot_entry {} {}", entry.TableId, entry.Position);
        auto const table = store.Tables.find(entry.TableId);
        if (table == store.Tables.end())
        {
            errors.push_back(where + " names a loot table that is not there");
            continue;
        }
        if (!positions.emplace(entry.TableId, entry.Position).second)
            errors.push_back(where + " is listed twice");
        if (!(entry.Chance > 0.0 && entry.Chance <= 1.0))
            errors.push_back(fmt::format("{} has the chance {}, which is not above 0 and at most 1", where, entry.Chance));
        if (entry.MinCount > entry.MaxCount)
            errors.push_back(fmt::format("{} gives from {} to {}, more at least than at most", where, entry.MinCount, entry.MaxCount));
        if (NeedsTemplate(entry.Type) && entry.TemplateId == 0)
            errors.push_back(fmt::format("{} is a {} entry with no template", where, TypeName(entry.Type)));
        table->second.push_back(entry);
    }
    for (CreatureLootRow const& creature : rows.Creatures)
    {
        if (!store.Tables.contains(creature.TableId))
        {
            errors.push_back(fmt::format("creature_loot gives template {} the loot table {}, which is not there", creature.TemplateId, creature.TableId));
            continue;
        }
        store.Creatures[creature.TemplateId].push_back(creature.TableId);
    }
    if (errors.size() != before)
        return std::nullopt;
    return store;
}

bool LootMgr::Load(std::vector<std::string>& errors)
{
    LootRows rows;
    RowSource source;
    {
        std::lock_guard const lock(_sourceMutex);
        source = _rowSource;
    }
    if (source)
        rows = source();
    else if (!ReadWorldRows(rows, errors))
        return false;

    std::optional<LootStore> next = BuildStore(rows, errors);
    if (!next)
        return false;
    std::size_t const tables = next->Tables.size();
    std::size_t const creatures = next->Creatures.size();
    _loot.Replace(std::move(*next));
    LOG_INFO(LootLog, "Loaded {} loot table(s) with {} entr(ies), and the tables of {} creature template(s)", tables, rows.Entries.size(), creatures);
    return true;
}

bool LootMgr::HasTable(std::string_view tableId) const
{
    return _loot.Get()->Tables.contains(tableId);
}

std::vector<std::string> LootMgr::GetCreatureTables(uint32 templateId) const
{
    auto const store = _loot.Get();
    auto const found = store->Creatures.find(templateId);
    return found == store->Creatures.end() ? std::vector<std::string>{} : found->second;
}

std::vector<LootDrop> LootMgr::Roll(std::string_view tableId, RequirementContext const& wizard) const
{
    thread_local std::mt19937_64 random(RandomEngine::Instance()());
    return Roll(tableId, wizard, random);
}

std::vector<LootDrop> LootMgr::Roll(std::string_view tableId, RequirementContext const& wizard, std::mt19937_64& random) const
{
    auto const store = _loot.Get();
    auto const table = store->Tables.find(tableId);
    if (table == store->Tables.end())
    {
        LOG_WARN(LootLog, "The loot table {} is not there, so it gives nothing", tableId);
        return {};
    }

    std::uniform_real_distribution<double> chance(0.0, 1.0);
    std::vector<LootDrop> drops;
    auto const give = [&](LootEntryRow const& entry)
    {
        uint32 const count = std::uniform_int_distribution<uint32>(entry.MinCount, entry.MaxCount)(random);
        if (count > 0)
            drops.push_back({ entry.Type, entry.TemplateId, count });
    };
    auto const eligible = [&wizard](LootEntryRow const& entry)
    {
        return !entry.RequirementListId || sRequirementMgr.Evaluate(*entry.RequirementListId, wizard);
    };

    std::map<uint8, std::vector<LootEntryRow const*>> groups;
    for (LootEntryRow const& entry : table->second)
    {
        if (!eligible(entry))
            continue;
        if (entry.Group != 0)
            groups[entry.Group].push_back(&entry);
        else if (double const scaled = ScaledChance(entry); scaled >= 1.0 || chance(random) < scaled)
            give(entry);
    }
    for (auto const& [group, entries] : groups)
    {
        double const roll = chance(random);
        double reached = 0.0;
        for (LootEntryRow const* entry : entries)
        {
            reached += ScaledChance(*entry);
            if (roll < reached)
            {
                give(*entry);
                break;
            }
        }
    }
    return drops;
}
