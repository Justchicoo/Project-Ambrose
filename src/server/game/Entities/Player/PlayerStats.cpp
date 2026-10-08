/*
 * Project Ambrose by Imjustchico
 * Builds a wizard's stats from its rows and refuses a wizard whose school has no level table, naming what is missing. A wizard with no character_stats row has earned the training points every level up to its own gives and stands at full health and mana; a stored health or mana above its maximum is brought down to it, and gold above the pouch its level allows is brought down to the pouch, which is how the client's own maximums are reached: health and energy are base plus bonus, and nothing a wizard wears adds a bonus yet. Live changes to health, mana, gold, power pips and potions stay within the same limits, gold handing back what the pouch cannot hold. A save writes health and mana as full, not as a number, whenever they stand at their maximum, so a wizard stays full when its base values change.
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
    stats._powerPip = static_cast<float>(row->PipChance);
    stats._shadowPipRating = row->ShadowPipRating;
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
    stats._stored.Gold = std::clamp(stats._stored.Gold, 0, std::max(row->Gold, 0));
    stats._stored.PotionMax = std::max(stats._stored.PotionMax, 0.0f);
    stats._stored.PotionCharge = std::clamp(stats._stored.PotionCharge, 0.0f, stats._stored.PotionMax);
    return stats;
}

bool PlayerStats::SetHealth(int32 value) noexcept
{
    int32 const bounded = std::clamp(value, 0, std::max(GetMaxHitpoints(), 0));
    if (_hitpoints == bounded)
        return false;
    _hitpoints = bounded;
    return true;
}

bool PlayerStats::SetMana(int32 value) noexcept
{
    int32 const bounded = std::clamp(value, 0, std::max(GetMaxMana(), 0));
    if (_mana == bounded)
        return false;
    _mana = bounded;
    return true;
}

bool PlayerStats::SetGold(int64 value) noexcept
{
    int64 const bounded = std::clamp<int64>(value, 0, std::max(_base.Gold, 0));
    int32 const gold = static_cast<int32>(bounded);
    if (_stored.Gold == gold)
        return false;
    _stored.Gold = gold;
    return true;
}

int64 PlayerStats::ModifyGold(int64 amount) noexcept
{
    int64 const before = _stored.Gold;
    int64 const maximum = std::max(_base.Gold, 0);
    int64 after = before;
    if (amount > 0)
        after = amount > maximum - before ? maximum : before + amount;
    else if (amount < 0)
        after = amount < -before ? 0 : before + amount;
    _stored.Gold = static_cast<int32>(after);
    return amount - (after - before);
}

bool PlayerStats::SetPotions(float charge, float maximum) noexcept
{
    if (!std::isfinite(charge) || !std::isfinite(maximum) || maximum < 0.0f)
        return false;
    float const boundedMaximum = maximum;
    float const boundedCharge = std::clamp(charge, 0.0f, boundedMaximum);
    if (_stored.PotionMax == boundedMaximum && _stored.PotionCharge == boundedCharge)
        return false;
    _stored.PotionMax = boundedMaximum;
    _stored.PotionCharge = boundedCharge;
    return true;
}

bool PlayerStats::UsePotion(double restoreFraction) noexcept
{
    if (_stored.PotionCharge < 1.0f || !std::isfinite(restoreFraction) || restoreFraction < 0.0 || restoreFraction > 1.0)
        return false;
    --_stored.PotionCharge;
    int64 const healthRestore = std::lround(static_cast<double>(GetMaxHitpoints()) * restoreFraction);
    int64 const manaRestore = std::lround(static_cast<double>(GetMaxMana()) * restoreFraction);
    SetHealth(static_cast<int32>(std::min<int64>(static_cast<int64>(_hitpoints) + healthRestore, std::numeric_limits<int32>::max())));
    SetMana(static_cast<int32>(std::min<int64>(static_cast<int64>(_mana) + manaRestore, std::numeric_limits<int32>::max())));
    return true;
}

bool PlayerStats::RefillPotion() noexcept
{
    if (_stored.PotionCharge >= _stored.PotionMax)
        return false;
    _stored.PotionCharge = std::min(_stored.PotionCharge + 1.0f, _stored.PotionMax);
    return true;
}

bool PlayerStats::SetPowerPip(float value) noexcept
{
    if (!std::isfinite(value) || value < 0.0f || _powerPip == value)
        return false;
    _powerPip = value;
    return true;
}

bool PlayerStats::SetShadowPipRating(float value) noexcept
{
    if (!std::isfinite(value) || value < 0.0f || _shadowPipRating == value)
        return false;
    _shadowPipRating = value;
    return true;
}

bool PlayerStats::SetShowItemLock(bool value) noexcept
{
    if (_stored.ShowItemLock == value)
        return false;
    _stored.ShowItemLock = value;
    return true;
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
        .Set("m_powerPipBase", _powerPip)
        .Set("m_pipConversionBaseAllSchools", _base.PipConversionAll)
        .Set("m_shadowPipRating", _shadowPipRating)
        .Set("m_archmasteryBase", _base.Archmastery)
        .Set("m_referenceLevel", _level)
        .Set("m_schoolID", _schoolId)
        .Set("m_secondarySchool", _stored.SecondarySchoolId);
    filler.Set("m_purchasedCustomEmotes1", _stored.PurchasedCustomEmotes[0])
        .Set("m_purchasedCustomEmotes2", _stored.PurchasedCustomEmotes[1])
        .Set("m_purchasedCustomEmotes3", _stored.PurchasedCustomEmotes[2])
        .Set("m_purchasedCustomTeleportEffects1", _stored.PurchasedCustomTeleportEffects[0])
        .Set("m_purchasedCustomTeleportEffects2", _stored.PurchasedCustomTeleportEffects[1])
        .Set("m_purchasedCustomTeleportEffects3", _stored.PurchasedCustomTeleportEffects[2])
        .Set("m_showItemLock", _stored.ShowItemLock);
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
