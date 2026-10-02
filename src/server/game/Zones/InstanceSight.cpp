/*
 * Project Ambrose by Imjustchico
 * Files every object of the instance where its spawn row places it, sized so a reach query looks at the cells around the viewer only, or one large cell when the whole zone is shown, and offers a viewer the ids within reach plus every exempt object, flagged as exempt.
 */

#include "InstanceSight.h"
#include "Map.h"

#include <algorithm>

InstanceSight::InstanceSight(Map const& map, VisibilityRange range)
    : _range(range), _places(range.ShowsEverything() ? WholeZoneCellSize : std::max(range.Reach(), Grid::MinimumCellSize))
{
    for (MapObject const& object : map.GetObjects())
    {
        _places.Place(object.GlobalId, { object.Spawn.Position.X, object.Spawn.Position.Y, object.Spawn.Position.Z });
        _objects.insert(object.GlobalId);
        if (object.ExemptFromAoi)
            _exempt.insert(object.GlobalId);
    }
}

void InstanceSight::PlaceWizard(uint64 worldGuid, GridPoint const& at)
{
    _places.Place(worldGuid, at);
}

std::vector<VisibilityCandidate> InstanceSight::CandidatesFor(uint64 viewer, GridPoint const& at) const
{
    std::vector<VisibilityCandidate> candidates;
    for (GridNeighbor const& near : _places.Near(at, _range.Reach()))
        if (near.Id != viewer)
            candidates.push_back({ near.Id, near.DistanceSquared, _exempt.contains(near.Id) });
    for (uint64 const id : _exempt)
        if (std::none_of(candidates.begin(), candidates.end(), [id](VisibilityCandidate const& candidate) { return candidate.Id == id; }))
            candidates.push_back({ id, 0.0f, true });
    return candidates;
}
