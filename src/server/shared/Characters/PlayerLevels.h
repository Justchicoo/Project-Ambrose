/*
 * Project Ambrose by Imjustchico
 * What a wizard's level and school decide: one row per school and level with the experience total that reaches the next level, the base health, mana, gold pouch, pip chance, training points, crafting slots, pet energy, pip conversion ratings, shadow pip rating, archmastery and level name; the magic schools with their secondary-school badges; MagicXPConfig's own settings, its encounter experience factors and the level each mob rank stands for; and PlayerLevelSet, an immutable snapshot that validates all of them once and answers a school's row for a level the way the client's own lookup does, the level cap's row for any level above it.
 */

#ifndef AMBROSE_PLAYERLEVELS_H
#define AMBROSE_PLAYERLEVELS_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct PlayerLevelInfo
{
    static constexpr std::size_t PipSchoolCount = 7;

    uint32 SchoolId = 0;
    uint32 Level = 0;
    int32 XpToLevel = 0;
    int32 Hitpoints = 0;
    int32 Mana = 0;
    int32 Gold = 0;
    float PipChance = 0.0f;
    int32 TrainingPoints = 0;
    int32 CraftingSlots = 0;
    int32 PetEnergy = 0;
    int32 PipConversionAll = 0;
    std::array<int32, PipSchoolCount> PipConversion{};
    float ShadowPipRating = 0.0f;
    float Archmastery = 0.0f;
    std::string LevelName;

    bool operator==(PlayerLevelInfo const&) const = default;
};

struct MagicSchool
{
    uint32 Id = 0;
    std::string Name;
    uint32 MinLevel = 0;
    int32 Index = 0;
    std::vector<std::string> Badges;

    bool operator==(MagicSchool const&) const = default;
};

struct ConfigValue
{
    std::string Name;
    double Value = 0.0;

    bool operator==(ConfigValue const&) const = default;
};

struct MobRankLevel
{
    int32 Rank = 0;
    int32 Level = 0;

    bool operator==(MobRankLevel const&) const = default;
};

struct PlayerLevelData
{
    std::vector<PlayerLevelInfo> Levels;
    std::vector<MagicSchool> Schools;
    std::vector<ConfigValue> XpConfig;
    std::vector<float> EncounterXpFactors;
    std::vector<MobRankLevel> MobRanks;

    bool Empty() const noexcept { return Levels.empty() && Schools.empty() && XpConfig.empty() && EncounterXpFactors.empty() && MobRanks.empty(); }
};

class PlayerLevelSet
{
public:
    static constexpr std::string_view MaxLevelSetting = "m_maxSchoolLevel";
    static constexpr std::array<std::string_view, PlayerLevelInfo::PipSchoolCount> PipSchools{ "Fire", "Ice", "Storm", "Life", "Myth", "Death", "Balance" };
    static constexpr uint32 MaxLevel = 65535;
    static constexpr std::size_t MaxSchoolNameBytes = 64;
    static constexpr std::size_t MaxBadgeBytes = 128;
    static constexpr std::size_t MaxLevelNameBytes = 128;
    static constexpr std::size_t MaxSettingNameBytes = 128;
    static constexpr std::size_t MaxBadges = 256;
    static constexpr std::size_t MaxEncounterFactors = 256;

    PlayerLevelSet() = default;
    PlayerLevelSet(PlayerLevelSet const&) = delete;
    PlayerLevelSet& operator=(PlayerLevelSet const&) = delete;

    static std::shared_ptr<PlayerLevelSet const> Build(PlayerLevelData data, std::vector<std::string>& errors);

    PlayerLevelData const& GetData() const noexcept { return _data; }
    bool IsEmpty() const noexcept { return _data.Empty(); }
    uint32 GetMaxLevel() const noexcept { return _maxLevel; }
    std::size_t GetLevelTableCount() const noexcept { return _levels.size(); }

    PlayerLevelInfo const* GetInfo(uint32 schoolId, int64 level) const noexcept;
    PlayerLevelInfo const* GetInfo(std::string_view school, int64 level) const noexcept;
    MagicSchool const* FindSchool(uint32 id) const noexcept;
    MagicSchool const* FindSchool(std::string_view name) const noexcept;
    std::optional<double> GetXpSetting(std::string_view name) const noexcept;
    int32 GetLevelForRank(int32 rank) const noexcept;

private:
    PlayerLevelData _data;
    uint32 _maxLevel = 0;
    std::map<uint32, std::vector<PlayerLevelInfo const*>> _levels;
    std::map<uint32, MagicSchool const*> _schoolsById;
    std::map<std::string, MagicSchool const*, std::less<>> _schoolsByName;
    std::map<std::string, double, std::less<>> _xpSettings;
};

#endif
