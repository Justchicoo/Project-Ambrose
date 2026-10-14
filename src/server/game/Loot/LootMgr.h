/*
 * Project Ambrose by Imjustchico
 * The loot manager (sLootMgr): the world's loot tables, their entries and the tables each creature template drops, read into one checked and reloadable generation, and the roller that gives a wizard a table's guaranteed entries, each free entry it wins and at most one entry of each group, skipping an entry whose requirement list the wizard fails and scaling a chance by the live Rate.Drop setting of its kind.
 */

#ifndef AMBROSE_LOOTMGR_H
#define AMBROSE_LOOTMGR_H

#include "ReloadableStore.h"
#include "Types.h"

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

class RequirementContext;

enum class LootType : uint8
{
    Gold,
    Xp,
    Item,
    Spell,
    Reagent,
    TreasureCard
};

struct LootEntryRow
{
    std::string TableId;
    uint32 Position = 0;
    LootType Type = LootType::Item;
    uint32 TemplateId = 0;
    uint32 MinCount = 1;
    uint32 MaxCount = 1;
    double Chance = 1.0;
    uint8 Group = 0;
    std::optional<std::string> RequirementListId;
};

struct CreatureLootRow
{
    uint32 TemplateId = 0;
    std::string TableId;
};

struct LootRows
{
    std::vector<std::string> Tables;
    std::vector<LootEntryRow> Entries;
    std::vector<CreatureLootRow> Creatures;
};

struct LootStore
{
    std::map<std::string, std::vector<LootEntryRow>, std::less<>> Tables;
    std::map<uint32, std::vector<std::string>> Creatures;
};

struct LootDrop
{
    LootType Type = LootType::Item;
    uint32 TemplateId = 0;
    uint32 Count = 0;
};

class LootMgr
{
public:
    static constexpr std::string_view Target = "loot";
    static constexpr std::string_view ResultClass = "class ResLoot";
    using RowSource = std::function<LootRows()>;

    static LootMgr& Instance();
    static std::optional<LootType> ParseType(std::string_view text);
    static std::string_view TypeName(LootType type);

    LootMgr() = default;
    LootMgr(LootMgr const&) = delete;
    LootMgr& operator=(LootMgr const&) = delete;

    void RegisterReloadTargets();
    void SetRowSource(RowSource source);
    bool Load(std::vector<std::string>& errors);
    bool HasTable(std::string_view tableId) const;
    std::vector<std::string> GetCreatureTables(uint32 templateId) const;
    std::vector<LootDrop> Roll(std::string_view tableId, RequirementContext const& wizard) const;
    std::vector<LootDrop> Roll(std::string_view tableId, RequirementContext const& wizard, std::mt19937_64& random) const;

private:
    static bool ReadWorldRows(LootRows& rows, std::vector<std::string>& errors);
    static std::optional<LootStore> BuildStore(LootRows const& rows, std::vector<std::string>& errors);

    ReloadableStore<LootStore> _loot;
    mutable std::mutex _sourceMutex;
    RowSource _rowSource;
};

#define sLootMgr LootMgr::Instance()

#endif
