/*
 * Project Ambrose by Imjustchico
 * Lets a Dynamod naming an object's zone tag decide whether the wizard sees it, over any requirements, and otherwise asks the requirement engine about each object with spawn requirements once for each step of a wizard's progress and leaves out the candidates the wizard does not qualify for; anything without requirements, a wizard among them, passes untouched, and a progress value other than the last one forgets every answer.
 */

#include "SightRequirements.h"
#include "Map.h"
#include "RequirementMgr.h"
#include "WizardDynaMods.h"

#include <utility>

std::vector<VisibilityCandidate> SightRequirements::Filter(std::vector<VisibilityCandidate> candidates, Map const& map, RequirementContext const& wizard, WizardDynaMods const& mods,
    uint64 progress)
{
    Follow(progress);
    std::erase_if(candidates, [&](VisibilityCandidate const& candidate) { return !Shows(map, candidate.Id, wizard, mods); });
    return candidates;
}

bool SightRequirements::Shows(Map const& map, uint64 id, RequirementContext const& wizard, WizardDynaMods const& mods)
{
    MapObject const* const object = map.FindObject(id);
    if (!object)
        return true;
    if (std::optional<bool> const modded = mods.Visibility(map.GetZonePath(), object->Spawn.Tag))
        return *modded;
    if (!object->SpawnRequirements)
        return true;
    auto const known = _answers.find(id);
    if (known != _answers.end())
        return known->second;
    bool const shown = sRequirementMgr.EvaluateTree(*object->SpawnRequirements, wizard);
    _answers.emplace(id, shown);
    return shown;
}

void SightRequirements::Clear()
{
    _progress.reset();
    _answers.clear();
}

void SightRequirements::Follow(uint64 progress)
{
    if (_progress == progress)
        return;
    _progress = progress;
    _answers.clear();
}
