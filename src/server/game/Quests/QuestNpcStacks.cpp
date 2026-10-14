/*
 * Project Ambrose by Imjustchico
 * Buckets each zone's placements into cells as wide as the radius, joins every two templates placed within the radius of each other in one zone, names each group by its lowest template id, and says whether two templates are placed in one zone.
 */

#include "QuestNpcStacks.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace
{
    uint32 Find(std::unordered_map<uint32, uint32>& parent, uint32 id)
    {
        auto found = parent.try_emplace(id, id).first;
        while (found->second != found->first)
        {
            auto const next = parent.find(found->second);
            found->second = next->second;
            found = next;
        }
        return found->first;
    }
}

QuestNpcStacks QuestNpcStacks::Build(std::vector<QuestPlacement> const& placements, float radius)
{
    using Cell = std::tuple<int64, int64, int64>;
    std::map<std::string, std::map<Cell, std::vector<QuestPlacement const*>>, std::less<>> zones;
    float const width = radius > 0.0f ? radius : 1.0f;
    auto const cellOf = [width](float value) { return static_cast<int64>(std::floor(value / width)); };
    for (QuestPlacement const& placement : placements)
        if (placement.TemplateId != 0)
            zones[placement.Zone][Cell{ cellOf(placement.X), cellOf(placement.Y), cellOf(placement.Z) }].push_back(&placement);

    std::unordered_map<uint32, uint32> parent;
    float const reach = radius * radius;
    for (auto const& [zone, cells] : zones)
        for (auto const& [cell, here] : cells)
            for (int64 dx = -1; dx <= 1; ++dx)
                for (int64 dy = -1; dy <= 1; ++dy)
                    for (int64 dz = -1; dz <= 1; ++dz)
                    {
                        auto const near = cells.find(Cell{ std::get<0>(cell) + dx, std::get<1>(cell) + dy, std::get<2>(cell) + dz });
                        if (near == cells.end())
                            continue;
                        for (QuestPlacement const* left : here)
                            for (QuestPlacement const* right : near->second)
                            {
                                if (left->TemplateId == right->TemplateId)
                                    continue;
                                float const x = left->X - right->X;
                                float const y = left->Y - right->Y;
                                float const z = left->Z - right->Z;
                                if (x * x + y * y + z * z > reach)
                                    continue;
                                uint32 const a = Find(parent, left->TemplateId);
                                uint32 const b = Find(parent, right->TemplateId);
                                if (a != b)
                                    parent[std::max(a, b)] = std::min(a, b);
                            }
                    }

    QuestNpcStacks stacks;
    for (QuestPlacement const& placement : placements)
        if (placement.TemplateId != 0)
            stacks.m_zones[placement.TemplateId].insert(placement.Zone);
    std::set<uint32> roots;
    for (auto const& [id, unused] : parent)
    {
        uint32 const root = Find(parent, id);
        stacks.m_root.emplace(id, root);
        roots.insert(root);
    }
    stacks.m_stacks = roots.size();
    return stacks;
}

uint32 QuestNpcStacks::StackOf(uint32 templateId) const
{
    auto const found = m_root.find(templateId);
    return found != m_root.end() ? found->second : templateId;
}

bool QuestNpcStacks::Same(uint32 left, uint32 right) const
{
    return StackOf(left) == StackOf(right);
}

std::optional<uint32> QuestNpcStacks::OneOf(std::vector<uint32> const& templateIds) const
{
    if (templateIds.empty())
        return std::nullopt;
    uint32 const stack = StackOf(templateIds.front());
    if (!std::all_of(templateIds.begin(), templateIds.end(), [this, stack](uint32 id) { return StackOf(id) == stack; }))
        return std::nullopt;
    return stack;
}

bool QuestNpcStacks::ShareZone(uint32 left, uint32 right) const
{
    auto const a = m_zones.find(left);
    auto const b = m_zones.find(right);
    if (a == m_zones.end() || b == m_zones.end())
        return false;
    return std::any_of(a->second.begin(), a->second.end(), [&b](std::string const& zone) { return b->second.contains(zone); });
}
