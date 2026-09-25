/*
 * Project Ambrose by Imjustchico
 * A wizard's stats while it plays: its level, experience and school from its character row, what character_stats keeps, and what its school's row for its level gives, the base health, mana, gold pouch, energy, power pip chance, shadow pip rating, archmastery and pip conversion rating, with the stat configuration's shadow pip limit; it fills the WizGameStats the player object carries and the ClientMagicSchoolBehavior that holds the level, experience and training points, and gives back the row a save writes.
 */

#ifndef AMBROSE_PLAYERSTATS_H
#define AMBROSE_PLAYERSTATS_H

#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "PlayerLevels.h"
#include "PropertyObject.h"
#include "StatEffects.h"

#include <optional>
#include <string>
#include <string_view>

class PlayerStats
{
public:
    static constexpr std::string_view ShadowPipMaxSetting = "m_shadowPipMax";

    static std::optional<PlayerStats> Create(CharacterSummary const& character, std::optional<CharacterStats> const& stored, PlayerLevelSet const& levels, StatEffectSet const& effects,
        std::string& problem);

    uint32 GetSchoolId() const noexcept { return _schoolId; }
    int32 GetLevel() const noexcept { return _level; }
    int32 GetExperience() const noexcept { return _experience; }
    PlayerLevelInfo const& GetBase() const noexcept { return _base; }
    std::optional<int32> GetShadowPipMax() const noexcept { return _shadowPipMax; }
    int32 GetMaxHitpoints() const noexcept { return _base.Hitpoints; }
    int32 GetMaxMana() const noexcept { return _base.Mana; }
    int32 GetHitpoints() const noexcept { return _hitpoints; }
    int32 GetMana() const noexcept { return _mana; }
    int32 GetGold() const noexcept { return _stored.Gold; }
    int32 GetTrainingPoints() const noexcept { return _stored.TrainingPoints; }

    CharacterStats ToStored() const;
    bool WriteGameStats(PropertyObject& gameStats, std::string& problem) const;
    bool WriteSchool(PropertyObject& behavior, std::string& problem) const;

private:
    uint32 _schoolId = 0;
    int32 _level = 0;
    int32 _experience = 0;
    PlayerLevelInfo _base;
    std::optional<int32> _shadowPipMax;
    CharacterStats _stored;
    int32 _hitpoints = 0;
    int32 _mana = 0;
};

#endif
