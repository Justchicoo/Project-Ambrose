/*
 * Project Ambrose by Imjustchico
 * Builds each mod as a DynaMod with its client tag, originator and index, the fields the transmit mask selects, and the full set as a DynaModList, both written in the binary format under the transmit mask as the client reads them; a change tells the client only of armed mods in the zone it stands in, dropping the old one before adding the new, and a mod names a state only when its state is not empty.
 */

#include "DynaModMessages.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "PropertyObject.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    PropertyObjectPtr Build(TypeCatalogPtr const& catalog, DynaMod const& mod, std::string& problem)
    {
        PropertyObjectPtr object = catalog ? PropertyObject::Create(catalog, DynaModMessages::ModClass) : nullptr;
        if (!object)
        {
            problem = fmt::format("the type dump has no {}", DynaModMessages::ModClass);
            return nullptr;
        }
        PropertyFiller(*object, problem).Set("m_clientTag", mod.Tag).Set("m_originator", mod.Originator).Set("m_index", mod.Index);
        return problem.empty() ? std::move(object) : nullptr;
    }

    std::optional<std::string> Encode(PropertyObject const& object, std::string& problem)
    {
        EncodeResult const encoded = ObjectSerializer::Encode(&object, SerializerOptions());
        if (!encoded.Ok())
        {
            problem = fmt::format("the {} does not encode: {}", object.GetClass().Name, encoded.Detail);
            return std::nullopt;
        }
        return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
    }
}

std::optional<GameMessages::DynaModBehaviorUpdateMods> DynaModMessages::All(TypeCatalogPtr const& catalog, uint64 player, std::vector<DynaMod> const& mods, std::string& problem)
{
    PropertyObjectPtr const list = catalog ? PropertyObject::Create(catalog, ListClass) : nullptr;
    if (!list)
    {
        problem = fmt::format("the type dump has no {}", ListClass);
        return std::nullopt;
    }
    PropertyValue::List built;
    built.reserve(mods.size());
    for (DynaMod const& mod : mods)
    {
        PropertyObjectPtr object = Build(catalog, mod, problem);
        if (!object)
            return std::nullopt;
        built.emplace_back(std::move(object));
    }
    PropertyFiller(*list, problem).Set("m_allMods", std::move(built));
    std::optional<std::string> encoded = problem.empty() ? Encode(*list, problem) : std::nullopt;
    if (!encoded)
        return std::nullopt;
    GameMessages::DynaModBehaviorUpdateMods message;
    message.GlobalId = player;
    message.UpdateAll = 1;
    message.AllMods = std::move(*encoded);
    return message;
}

std::optional<GameMessages::DynaModBehaviorUpdateMods> DynaModMessages::Change(TypeCatalogPtr const& catalog, uint64 player, std::string_view zone, DynaModChange const& change,
    std::string& problem)
{
    auto const armed = [zone](std::optional<DynaMod> const& mod) { return mod && !mod->Remove && mod->Zone == zone; };
    GameMessages::DynaModBehaviorUpdateMods message;
    message.GlobalId = player;
    if (armed(change.Removed))
    {
        message.Remove = 1;
        message.Index = change.Removed->Index;
    }
    if (armed(change.Added))
    {
        PropertyObjectPtr const object = Build(catalog, *change.Added, problem);
        std::optional<std::string> encoded = object ? Encode(*object, problem) : std::nullopt;
        if (!encoded)
            return std::nullopt;
        message.Add = 1;
        message.NewMod = std::move(*encoded);
    }
    if (!message.Add && !message.Remove)
        return std::nullopt;
    return message;
}

std::optional<GameMessages::EnterState> DynaModMessages::State(uint64 object, DynaMod const& mod)
{
    if (mod.Remove || mod.State.empty())
        return std::nullopt;
    GameMessages::EnterState message;
    message.GameObjectId = object;
    message.State = StringHash::StringId(mod.State);
    return message;
}
