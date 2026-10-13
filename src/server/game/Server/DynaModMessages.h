/*
 * Project Ambrose by Imjustchico
 * The messages that show one wizard its own Dynamods: MSG_DYNAMODBEHAVIOR_UPDATEMODS loading the mods its client arms spawners for in the zone it stands in, adding a new one and dropping a replaced one by its index, and MSG_ENTERSTATE putting a tagged object in the state a mod names.
 */

#ifndef AMBROSE_DYNAMODMESSAGES_H
#define AMBROSE_DYNAMODMESSAGES_H

#include "GameMessages.h"
#include "TypeRegistry.h"
#include "WizardDynaMods.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DynaModMessages
{
    inline constexpr std::string_view ListClass = "class DynaModList";
    inline constexpr std::string_view ModClass = "class DynaMod";

    std::optional<GameMessages::DynaModBehaviorUpdateMods> All(TypeCatalogPtr const& catalog, uint64 player, std::vector<DynaMod> const& mods, std::string& problem);
    std::optional<GameMessages::DynaModBehaviorUpdateMods> Change(TypeCatalogPtr const& catalog, uint64 player, std::string_view zone, DynaModChange const& change, std::string& problem);
    std::optional<GameMessages::EnterState> State(uint64 object, DynaMod const& mod);
}

#endif
