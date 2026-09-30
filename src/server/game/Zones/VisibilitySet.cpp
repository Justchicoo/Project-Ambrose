/*
 * Project Ambrose by Imjustchico
 * Works one update out from the objects near a wizard: each one inside the distance, or anywhere when exempt or when everything is shown, comes into view; one already in view stays while it is inside the distance and the band past it; every other one in view leaves it, including one no longer near at all; each list is given in ascending id order and the wizard itself is never among them.
 */

#include "VisibilitySet.h"

#include <algorithm>
#include <map>

VisibilityRange VisibilityRange::Resolve(float distanceSetting, float hysteresisSetting, std::optional<float> zoneFarClip) noexcept
{
    VisibilityRange range;
    range.Distance = distanceSetting > 0.0f ? distanceSetting : (zoneFarClip && *zoneFarClip > 0.0f ? *zoneFarClip : 0.0f);
    range.Hysteresis = hysteresisSetting > 0.0f ? hysteresisSetting : 0.0f;
    return range;
}

VisibilityChanges VisibilitySet::Update(std::vector<VisibilityCandidate> const& nearby, VisibilityRange const& range)
{
    VisibilityChanges changes;
    float const enter = range.Distance * range.Distance;
    float const stay = range.Reach() * range.Reach();
    std::map<uint64, bool> shown;
    for (VisibilityCandidate const& candidate : nearby)
    {
        bool const inView = _visible.contains(candidate.Id);
        bool const show = range.ShowsEverything() || candidate.Exempt || candidate.DistanceSquared <= (inView ? stay : enter);
        auto const [at, inserted] = shown.emplace(candidate.Id, show);
        if (!inserted)
            at->second = at->second || show;
    }
    for (auto const& [id, show] : shown)
    {
        if (!show || _visible.contains(id))
            continue;
        _visible.insert(id);
        if (_known.insert(id).second)
            changes.New.push_back(id);
        else
            changes.Added.push_back(id);
    }
    for (auto it = _visible.begin(); it != _visible.end();)
    {
        auto const found = shown.find(*it);
        if (found != shown.end() && found->second)
        {
            ++it;
            continue;
        }
        changes.Removed.push_back(*it);
        it = _visible.erase(it);
    }
    return changes;
}

VisibilityChanges VisibilitySet::Update(Grid const& grid, uint64 viewer, std::vector<uint64> const& exempt, VisibilityRange const& range)
{
    GridPoint const* const at = grid.Find(viewer);
    std::vector<VisibilityCandidate> nearby;
    if (at)
    {
        for (GridNeighbor const& neighbor : grid.Near(*at, range.Reach()))
            if (neighbor.Id != viewer)
                nearby.push_back({ neighbor.Id, neighbor.DistanceSquared, false });
    }
    for (uint64 const id : exempt)
        if (id != viewer && grid.Contains(id))
            nearby.push_back({ id, 0.0f, true });
    return Update(nearby, range);
}

bool VisibilitySet::Forget(uint64 id)
{
    bool const had = _known.erase(id) > 0;
    _visible.erase(id);
    return had;
}

void VisibilitySet::Clear()
{
    _known.clear();
    _visible.clear();
}
