/*
 * Project Ambrose by Imjustchico
 * Which of a zone instance's objects one wizard qualifies to see: an object its zone placed with spawn requirements is offered to the wizard's visibility set only while the wizard meets them, each answer kept until the wizard's progress changes, so a wizard sees the world its own quests have made and an object appears or leaves the moment that changes.
 */

#ifndef AMBROSE_SIGHTREQUIREMENTS_H
#define AMBROSE_SIGHTREQUIREMENTS_H

#include "Types.h"
#include "VisibilitySet.h"

#include <map>
#include <optional>
#include <vector>

class Map;
class RequirementContext;

class SightRequirements
{
public:
    std::vector<VisibilityCandidate> Filter(std::vector<VisibilityCandidate> candidates, Map const& map, RequirementContext const& wizard, uint64 progress);
    void Clear();

private:
    bool Shows(Map const& map, uint64 id, RequirementContext const& wizard);
    void Follow(uint64 progress);

    std::optional<uint64> _progress;
    std::map<uint64, bool> _answers;
};

#endif
