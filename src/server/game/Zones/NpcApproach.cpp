/*
 * Project Ambrose by Imjustchico
 * Picks the zone object a name or id means, preferring whole-name matches and then the one nearest the wizard, and works out the place a given distance from it on the wizard's side, with the yaw that faces it, since a wizard walks along (-sin yaw, -cos yaw).
 */

#include "NpcApproach.h"

#include "StringUtil.h"

#include <fmt/format.h>

#include <cmath>
#include <limits>

namespace
{
    std::vector<std::string> NamesOf(ZoneObjectSpawn const& object, NpcApproach::TemplateName const& templateName)
    {
        std::vector<std::string> names;
        for (std::string const& name : { object.OverrideName, object.Tag, templateName ? templateName(object) : std::string() })
            if (!name.empty())
                names.push_back(Ambrose::ToLower(name));
        return names;
    }

    float DistanceSquared(ZoneObjectSpawn const& object, PlayerPosition const& from)
    {
        float const dx = object.Position.X - from.X;
        float const dy = object.Position.Y - from.Y;
        return dx * dx + dy * dy;
    }
}

NpcStand NpcApproach::StandBeside(std::vector<ZoneObjectSpawn> const& objects, std::string_view wanted, TemplateName const& templateName, PlayerPosition const& from,
    float distance)
{
    NpcStand stand;
    std::string const lowered = Ambrose::ToLower(wanted);
    std::optional<uint64> const id = Ambrose::StringTo<uint64>(wanted);
    std::vector<ZoneObjectSpawn const*> whole;
    std::vector<ZoneObjectSpawn const*> partial;
    for (ZoneObjectSpawn const& object : objects)
    {
        if (id && (object.TemplateId == *id || object.Id == *id))
        {
            whole.push_back(&object);
            continue;
        }
        bool isWhole = false;
        bool isPartial = false;
        for (std::string const& name : NamesOf(object, templateName))
        {
            isWhole = isWhole || name == lowered;
            isPartial = isPartial || name.find(lowered) != std::string::npos;
        }
        if (isWhole)
            whole.push_back(&object);
        else if (isPartial && !lowered.empty())
            partial.push_back(&object);
    }
    std::vector<ZoneObjectSpawn const*> const& found = whole.empty() ? partial : whole;
    stand.Matches = found.size();
    if (found.empty())
    {
        stand.Problem = "nothing placed there has that name, template id or global id";
        return stand;
    }
    ZoneObjectSpawn const* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (ZoneObjectSpawn const* object : found)
        if (float const d = DistanceSquared(*object, from); d < best)
        {
            best = d;
            nearest = object;
        }
    std::string const fromTemplate = templateName ? templateName(*nearest) : std::string();
    if (!nearest->OverrideName.empty())
        stand.Name = nearest->OverrideName;
    else if (!fromTemplate.empty())
        stand.Name = fromTemplate;
    else
        stand.Name = fmt::format("template {}", nearest->TemplateId);
    float dx = from.X - nearest->Position.X;
    float dy = from.Y - nearest->Position.Y;
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f)
    {
        dx = 1.0f;
        dy = 0.0f;
    }
    else
    {
        dx /= length;
        dy /= length;
    }
    PlayerPosition place;
    place.X = nearest->Position.X + dx * distance;
    place.Y = nearest->Position.Y + dy * distance;
    place.Z = nearest->Position.Z;
    place.Yaw = std::atan2(dx, dy);
    stand.Place = place;
    return stand;
}
