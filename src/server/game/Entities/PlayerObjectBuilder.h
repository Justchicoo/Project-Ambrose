/*
 * Project Ambrose by Imjustchico
 * Builds the game object a wizard stands in the world as, the WizClientObject MSG_LOGINCOMPLETE carries: the CoreObject header the core object table gives its class with the player's template id, the wizard's id as both its global and its character id, where it stands and faces, the mobile id its zone instance gave it, one behavior for each the player's template names in the template's own order, built as the class the client makes for that behavior or left empty where the client takes it empty, the look and name filled from the stored wizard, its school behavior and the stats object the player carries filled from its stats, and its spellbook behavior holding a tracker for each spell it knows, in the order it learned them.
 */

#ifndef AMBROSE_PLAYEROBJECTBUILDER_H
#define AMBROSE_PLAYEROBJECTBUILDER_H

#include "CharacterSummary.h"
#include "CoreObjectSerializer.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PlayerStats.h"
#include "SpellTracker.h"

#include <string>
#include <string_view>
#include <vector>

struct PlayerPlacement
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Yaw = 0.0f;
    uint16 MobileId = 0;
};

class PlayerObjectBuilder
{
public:
    static constexpr std::string_view PlayerClass = "class WizClientObject";
    static constexpr std::string_view StatsClass = "class WizGameStats";

    PlayerObjectBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& playerTemplate,
        CharacterSummary const& character, PlayerStats const& stats, std::vector<SpellTracker> const& spells, PlayerPlacement const& placement, std::string& problem);
};

#endif
