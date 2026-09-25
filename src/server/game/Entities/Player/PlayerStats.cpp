/*
 * Project Ambrose by Imjustchico
 * Builds a wizard's stats from its rows and refuses a wizard whose school has no level table, naming what is missing. A wizard with no character_stats row has earned the training points every level up to its own gives and stands at full health and mana; a stored health or mana above its maximum is brought down to it, and gold above the pouch its level allows is brought down to the pouch, which is how the client's own maximums are reached: health and energy are base plus bonus, and nothing a wizard wears adds a bonus yet. A save writes health and mana as full, not as a number, whenever they stand at their maximum, so a wizard stays full when its base values change.
 */

#include "PlayerStats.h"
#include "PropertyFiller.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <limits>

std::optional<PlayerStats> PlayerStats::Create(CharacterSummary const& character, std::optional<CharacterStats> const& stored, PlayerLevelSet const& levels, StatEffectSet const& effects,
    std::string& problem)
{
    PlayerLevelInfo const* const row = levels.GetInfo(character.SchoolId, character.Level);
    if (!row)
    {
        if (levels.IsEmpty())
            problem = "the world database has no level tables, so no wizard has base stats; run the extractor's levels command against your install";
        else if (character.Level < 0)
            problem = fmt::format("wizard {} is at level {}", character.Guid, character.Level);
        else
            problem = fmt::format("wizard {}'s school {} has no rows in player_level_stats", character.Guid, character.SchoolId);
        return std::nullopt;
    }
    PlayerStats stats;
    stats._schoolId = character.SchoolId;
    stats._level = character.Level;
    stats._experience = character.Experience;
    stats._base = *row;
    if (std::optional<double> const limit = effects.Get(ShadowPipMaxSetting); limit && *limit >= 0.0 && *limit <= std::numeric_limits<int32>::max() && *limit == std::floor(*limit))
        stats._shadowPipMax = static_cast<int32>(*limit);
    stats._stored = stored.value_or(CharacterStats{});
    if (!stored)
    {
        int64 earned = 0;
        for (int32 level = 1; level <= character.Level; ++level)
            if (PlayerLevelInfo const* const reached = levels.GetInfo(character.SchoolId, level); reached && reached->Level == static_cast<uint32>(level))
                earned += std::max(reached->TrainingPoints, 0);
        stats._stored.TrainingPoints = static_cast<int32>(std::min<int64>(earned, std::numeric_limits<int32>::max()));
    }
    int32 const maxHitpoints = stats.GetMaxHitpoints();
    int32 const maxMana = stats.GetMaxMana();
    stats._hitpoints = stats._stored.Health ? std::clamp(*stats._stored.Health, 0, std::max(maxHitpoints, 0)) : maxHitpoints;
    stats._mana = stats._stored.Mana ? std::clamp(*stats._stored.Mana, 0, std::max(maxMana, 0)) : maxMana;
    if (row->Gold > 0)
        stats._stored.Gold = std::clamp(stats._stored.Gold, 0, row->Gold);
    return stats;
}

CharacterStats PlayerStats::ToStored() const
{
    CharacterStats stored = _stored;
    stored.Health = _hitpoints == GetMaxHitpoints() ? std::nullopt : std::optional<int32>(_hitpoints);
    stored.Mana = _mana == GetMaxMana() ? std::nullopt : std::optional<int32>(_mana);
    return stored;
}

bool PlayerStats::WriteGameStats(PropertyObject& gameStats, std::string& problem) const
{
    PropertyFiller filler(gameStats, problem);
    filler.Set("m_baseHitpoints", _base.Hitpoints)
        .Set("m_baseMana", _base.Mana)
        .Set("m_baseGoldPouch", _base.Gold)
        .Set("m_energyMax", _base.PetEnergy)
        .Set("m_currentHitpoints", _hitpoints)
        .Set("m_currentGold", _stored.Gold)
        .Set("m_currentMana", _mana)
        .Set("m_currentArenaPoints", _stored.ArenaPoints)
        .Set("m_potionMax", _stored.PotionMax)
        .Set("m_potionCharge", _stored.PotionCharge)
        .Set("m_powerPipBase", _base.PipChance)
        .Set("m_pipConversionBaseAllSchools", _base.PipConversionAll)
        .Set("m_shadowPipRating", _base.ShadowPipRating)
        .Set("m_archmasteryBase", _base.Archmastery)
        .Set("m_referenceLevel", _level)
        .Set("m_schoolID", _schoolId)
        .Set("m_secondarySchool", _stored.SecondarySchoolId);
    if (_shadowPipMax)
        filler.Set("m_shadowPipMax", *_shadowPipMax);
    return problem.empty();
}

bool PlayerStats::WriteSchool(PropertyObject& behavior, std::string& problem) const
{
    PropertyFiller(behavior, problem)
        .Set("m_schoolOfFocus", _schoolId)
        .Set("m_level", _level)
        .Set("m_experiencePoints", _experience)
        .Set("m_trainingPoints", _stored.TrainingPoints)
        .Set("m_overflowXP", _stored.OverflowXp)
        .Set("m_levelLocked", int32{ _stored.LevelLocked ? 1 : 0 })
        .Set("m_secondarySchool", _stored.SecondarySchoolId);
    return problem.empty();
}
