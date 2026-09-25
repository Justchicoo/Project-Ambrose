/*
 * Project Ambrose by Imjustchico
 * What character_stats keeps for a wizard beyond the level, experience and school its character row holds: experience past the level cap, its secondary school, unspent training points, gold, its current health and mana, each empty when full, potion charge and capacity, arena points and whether its level is locked, with the revision of the write that last set them.
 */

#ifndef AMBROSE_CHARACTERSTATS_H
#define AMBROSE_CHARACTERSTATS_H

#include "Types.h"

#include <optional>

struct CharacterStats
{
    int32 OverflowXp = 0;
    uint32 SecondarySchoolId = 0;
    int32 TrainingPoints = 0;
    int32 Gold = 0;
    std::optional<int32> Health;
    std::optional<int32> Mana;
    float PotionCharge = 0.0f;
    float PotionMax = 0.0f;
    int32 ArenaPoints = 0;
    bool LevelLocked = false;
    uint64 Revision = 0;

    bool operator==(CharacterStats const&) const = default;
};

#endif
