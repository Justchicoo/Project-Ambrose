/*
 * Project Ambrose by Imjustchico
 * A wizard's stats while it plays: its level, experience and school from its character row, what character_stats keeps, and what its school's row for its level gives, the base health, mana, gold pouch, energy, power pip chance, shadow pip rating, archmastery and pip conversion rating, with the stat configuration's shadow pip limit; it changes health and mana within their maximums, gold within the pouch, telling what did not fit, and the potion's charge, where a potion drunk with less than one whole charge changes nothing, and the power pip chance and shadow pip rating the session gives it until it leaves; it fills the WizGameStats the player object carries and the ClientMagicSchoolBehavior that holds the level, experience and training points, and gives back the row a save writes.
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

struct GoldChange
{
    int32 Gold = 0;
    int64 Overflow = 0;
};

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
    int32 GetGoldPouch() const noexcept { return _base.Gold; }
    float GetPotionCharge() const noexcept { return _stored.PotionCharge; }
    float GetPotionMax() const noexcept { return _stored.PotionMax; }
    float GetPowerPip() const noexcept { return _powerPip; }
    float GetShadowPipRating() const noexcept { return _shadowPipRating; }
    int32 SetHitpoints(int32 hitpoints) noexcept;
    int32 SetMana(int32 mana) noexcept;
    GoldChange ModifyGold(int64 delta) noexcept;
    float SetPotionCharge(float charge) noexcept;
    bool UsePotion(float restoreFraction) noexcept;
    void SetPowerPip(float chance) noexcept { _powerPip = chance; }
    void SetShadowPipRating(float rating) noexcept { _shadowPipRating = rating; }
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
    float _powerPip = 0.0f;
    float _shadowPipRating = 0.0f;
};

#endif
