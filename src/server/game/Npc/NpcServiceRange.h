/*
 * Project Ambrose by Imjustchico
 * Which NPCs a wizard stands in the service range of: an NPC is entered when the wizard comes within its radius and left only once the wizard is more than ExitMargin past it, or the NPC is gone or no longer offers anything, so a wizard walking about inside a range, or along its edge, crosses it once each way however many moves it sends. An NPC that offers nothing, no provider giving it an option, is never entered, so it shows no prompt and has none taken away. A radius is the provider's own when one sets it, otherwise Npc.InteractRadiusDefault, read live at each update.
 */

#ifndef AMBROSE_NPCSERVICERANGE_H
#define AMBROSE_NPCSERVICERANGE_H

#include "Types.h"

#include <optional>
#include <set>
#include <span>
#include <vector>

struct NpcPoint
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

class NpcServiceRange
{
public:
    static constexpr float ExitMargin = 25.0f;

    struct Npc
    {
        uint64 GlobalId = 0;
        NpcPoint Position;
        float Radius = 0.0f;
        bool Offers = true;
    };

    struct Changes
    {
        std::vector<uint64> Entered;
        std::vector<uint64> Left;
    };

    static float ResolveRadius(std::optional<float> own);

    Changes Update(NpcPoint const& wizard, std::span<Npc const> npcs);
    bool IsInside(uint64 globalId) const noexcept { return _inside.contains(globalId); }
    std::set<uint64> const& GetInside() const noexcept { return _inside; }
    void Clear() noexcept { _inside.clear(); }

private:
    std::set<uint64> _inside;
};

#endif
