/*
 * Project Ambrose by Imjustchico
 * The level and stat tables of the world database (sPlayerLevelMgr): player_level_stats read in one snapshot with the magic schools, their badges and MagicXPConfig's settings, encounter factors and mob ranks and validated whole as one level set, and WizStatisticEffectConfig's settings and bands as one stat set, each swapped in whole so a reader keeps the set it started with, a build that fails keeping the set that was serving and naming every row at fault, and each reloading on its own.
 */

#ifndef AMBROSE_PLAYERLEVELMGR_H
#define AMBROSE_PLAYERLEVELMGR_H

#include "PlayerLevels.h"
#include "ReloadableStore.h"
#include "StatEffects.h"
#include "Types.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct PlayerLevelLoadResult
{
    bool Loaded = false;
    bool Empty = false;
    std::size_t Schools = 0;
    std::size_t LevelTables = 0;
    uint32 MaxLevel = 0;
    std::size_t Levels = 0;
    std::size_t StatSettings = 0;
    std::size_t BandValues = 0;
    std::vector<std::string> Errors;
};

class PlayerLevelMgr
{
public:
    static constexpr std::string_view LevelTarget = "player_level_stats";
    static constexpr std::string_view StatTarget = "stat_effect_config";

    static PlayerLevelMgr& Instance();

    PlayerLevelMgr() = default;
    PlayerLevelMgr(PlayerLevelMgr const&) = delete;
    PlayerLevelMgr& operator=(PlayerLevelMgr const&) = delete;

    void RegisterReloadTargets();
    bool LoadLevels(std::vector<std::string>& errors);
    bool LoadStats(std::vector<std::string>& errors);
    PlayerLevelLoadResult Load();

    std::shared_ptr<PlayerLevelSet const> GetLevels() const { return _levels.Get(); }
    std::shared_ptr<StatEffectSet const> GetStats() const { return _stats.Get(); }
    uint64 GetLevelGeneration() const noexcept { return _levels.GetGeneration(); }
    uint64 GetStatGeneration() const noexcept { return _stats.GetGeneration(); }
    void Clear();

private:
    ReloadableStore<PlayerLevelSet> _levels;
    ReloadableStore<StatEffectSet> _stats;
};

#define sPlayerLevelMgr PlayerLevelMgr::Instance()

#endif
