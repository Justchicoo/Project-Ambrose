/*
 * Project Ambrose by Imjustchico
 * Tracks the NPC ranges a wizard stands in, entering at the radius and leaving past it by the margin.
 */

#include "NpcServiceRange.h"
#include "Settings.h"

#include <algorithm>
#include <cmath>

float NpcServiceRange::ResolveRadius(std::optional<float> own)
{
    if (own && std::isfinite(*own) && *own > 0.0f)
        return *own;
    return sSettings.Get<float>("Npc.InteractRadiusDefault");
}

NpcServiceRange::Changes NpcServiceRange::Update(NpcPoint const& wizard, std::span<Npc const> npcs)
{
    Changes changes;
    std::set<uint64> present;
    for (Npc const& npc : npcs)
    {
        present.insert(npc.GlobalId);
        float const dx = npc.Position.X - wizard.X;
        float const dy = npc.Position.Y - wizard.Y;
        float const dz = npc.Position.Z - wizard.Z;
        float const distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        bool const inside = _inside.contains(npc.GlobalId);
        if (!inside && npc.Offers && distance <= npc.Radius)
        {
            _inside.insert(npc.GlobalId);
            changes.Entered.push_back(npc.GlobalId);
        }
        else if (inside && (!npc.Offers || distance > npc.Radius + ExitMargin))
        {
            _inside.erase(npc.GlobalId);
            changes.Left.push_back(npc.GlobalId);
        }
    }
    for (auto it = _inside.begin(); it != _inside.end();)
    {
        if (present.contains(*it))
        {
            ++it;
            continue;
        }
        changes.Left.push_back(*it);
        it = _inside.erase(it);
    }
    return changes;
}
