/*
 * Project Ambrose by Imjustchico
 * A stored wizard as the character screens use it: identity and owning account, name parts or a custom name, school, level, experience, world, zone and position, times in Unix seconds, online and soft-delete state, and the appearance kept one field per WizardCharacterBehavior property.
 */

#ifndef AMBROSE_CHARACTERSUMMARY_H
#define AMBROSE_CHARACTERSUMMARY_H

#include "Types.h"

#include <optional>
#include <string>

struct CharacterAppearance
{
    uint32 BehaviorTemplateNameId = 0;
    uint32 Gender = 0;
    uint32 Race = 0;
    uint8 HeadHandsModel = 0;
    uint8 HairModel = 0;
    uint8 HatModel = 0;
    uint8 TorsoModel = 0;
    uint8 FeetModel = 0;
    uint8 WandModel = 0;
    uint8 SkinColor = 0;
    uint8 SkinDecal = 0;
    uint8 HairColor = 0;
    uint8 HatColor = 0;
    uint8 HatDecal = 0;
    uint8 TorsoColor = 0;
    uint8 TorsoDecal = 0;
    uint8 TorsoDecal2 = 0;
    uint8 FeetColor = 0;
    uint8 FeetDecal = 0;
    uint16 SkinDecal2 = 0;
    uint8 ExtendedHairColor = 0;
    uint16 ExtendedSkinDecal = 0;
    uint8 AfterCombatDance = 0;
    uint32 AfterCombatVictoryDance = 0;
    uint32 NewPlayerOptions = 0;
    uint32 NewPlayerOptions2 = 0;

    bool operator==(CharacterAppearance const&) const = default;
};

struct CharacterSummary
{
    uint64 Guid = 0;
    uint64 Account = 0;
    uint32 NameIndices = 0;
    std::optional<std::string> CustomName;
    bool ShouldRename = false;
    uint32 SchoolId = 0;
    int32 Level = 1;
    int32 Experience = 0;
    int32 World = 0;
    std::string Zone;
    std::string ZoneDisplay;
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float Orientation = 0.0f;
    uint64 Created = 0;
    uint64 LastLogout = 0;
    bool Online = false;
    std::optional<uint64> DeletedAt;
    std::optional<uint64> DeletedAccount;
    CharacterAppearance Appearance;

    bool IsDeleted() const noexcept { return DeletedAt.has_value(); }
    bool operator==(CharacterSummary const&) const = default;
};

#endif
