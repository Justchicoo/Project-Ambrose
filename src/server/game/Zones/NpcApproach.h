/*
 * Project Ambrose by Imjustchico
 * Finds where a wizard should stand to speak with an object placed in its zone: the object is named by its override name, zone tag or template name, in any case, or by its template or global id, a whole-name match wins over one that only holds the name, the one nearest the wizard wins among several, and the place is a given distance from it on the wizard's side, at its height, facing it.
 */

#ifndef AMBROSE_NPCAPPROACH_H
#define AMBROSE_NPCAPPROACH_H

#include "PlayerMovement.h"
#include "ZoneMgr.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct NpcStand
{
    std::optional<PlayerPosition> Place;
    std::string Name;
    std::size_t Matches = 0;
    std::string Problem;
};

namespace NpcApproach
{
    using TemplateName = std::function<std::string(ZoneObjectSpawn const&)>;

    NpcStand StandBeside(std::vector<ZoneObjectSpawn> const& objects, std::string_view wanted, TemplateName const& templateName, PlayerPosition const& from,
        float distance);
}

#endif
