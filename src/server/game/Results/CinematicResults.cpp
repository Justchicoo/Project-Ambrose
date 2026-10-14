/*
 * Project Ambrose by Imjustchico
 * Reads a ResPostEvent's m_eventName and a ResStartStagedCinematic's m_cinematicName, m_stageName and m_bIncludeAllPlayersInZone, and a ResTeleport's m_destinationZone and m_destinationLoc, giving nothing for one that names no event, cinematic or zone.
 */

#include "CinematicResults.h"
#include "PropertyObject.h"

namespace
{
    std::string TextOf(PropertyObject const& result, std::string_view property)
    {
        PropertyValue const* const value = result.Get(property);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }
}

namespace CinematicResults
{
    std::optional<std::string> EventOf(PropertyObject const& result)
    {
        std::string event = TextOf(result, "m_eventName");
        if (event.empty())
            return std::nullopt;
        return event;
    }

    std::optional<StagedCinematicResult> StagedCinematicOf(PropertyObject const& result)
    {
        StagedCinematicResult cinematic;
        cinematic.Cinematic = TextOf(result, "m_cinematicName");
        if (cinematic.Cinematic.empty())
            return std::nullopt;
        cinematic.Stage = TextOf(result, "m_stageName");
        PropertyValue const* const all = result.Get("m_bIncludeAllPlayersInZone");
        bool const* const wholeZone = all ? all->GetIf<bool>() : nullptr;
        cinematic.WholeZone = wholeZone && *wholeZone;
        return cinematic;
    }

    std::optional<TeleportResult> TeleportOf(PropertyObject const& result)
    {
        TeleportResult teleport{ TextOf(result, "m_destinationZone"), TextOf(result, "m_destinationLoc") };
        if (teleport.Zone.empty())
            return std::nullopt;
        return teleport;
    }
}
