/*
 * Project Ambrose by Imjustchico
 * Groups the object templates the zones place on one spot, the copies of one NPC that the game spawns in turn as a player's quests move on, each with its own template id, so the quest import counts every copy as the same NPC when it names givers and turn-ins, and keeps the zones each template is placed in.
 */

#ifndef AMBROSE_QUESTNPCSTACKS_H
#define AMBROSE_QUESTNPCSTACKS_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct QuestPlacement
{
    std::string Zone;
    uint32 TemplateId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

class QuestNpcStacks
{
public:
    static constexpr float DefaultRadius = 5.0f;

    static QuestNpcStacks Build(std::vector<QuestPlacement> const& placements, float radius = DefaultRadius);

    uint32 StackOf(uint32 templateId) const;
    bool Same(uint32 left, uint32 right) const;
    std::optional<uint32> OneOf(std::vector<uint32> const& templateIds) const;
    bool ShareZone(uint32 left, uint32 right) const;
    std::size_t GetStackCount() const { return m_stacks; }
    std::size_t GetStackedTemplates() const { return m_root.size(); }

private:
    std::unordered_map<uint32, uint32> m_root;
    std::unordered_map<uint32, std::set<std::string, std::less<>>> m_zones;
    std::size_t m_stacks = 0;
};

#endif
