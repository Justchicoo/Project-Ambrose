/*
 * Project Ambrose by Imjustchico
 * Reads the results that drive a quest's cinematics and beats: the event a ResPostEvent posts into the wizard's zone and the staged cinematic a ResStartStagedCinematic starts, with its stage and whether everyone in the zone sees it, and the zone and place a quest's ResTeleport sends the wizard to, which a zone door's ResTeleport leaves to zone_teleport.
 */

#ifndef AMBROSE_CINEMATICRESULTS_H
#define AMBROSE_CINEMATICRESULTS_H

#include <optional>
#include <string>
#include <string_view>

class PropertyObject;

struct StagedCinematicResult
{
    std::string Cinematic;
    std::string Stage;
    bool WholeZone = false;
};

struct TeleportResult
{
    std::string Zone;
    std::string Location;
};

namespace CinematicResults
{
    inline constexpr std::string_view PostEventClass = "class ResPostEvent";
    inline constexpr std::string_view StagedCinematicClass = "class ResStartStagedCinematic";
    inline constexpr std::string_view TeleportClass = "class ResTeleport";

    std::optional<std::string> EventOf(PropertyObject const& result);
    std::optional<StagedCinematicResult> StagedCinematicOf(PropertyObject const& result);
    std::optional<TeleportResult> TeleportOf(PropertyObject const& result);
}

#endif
