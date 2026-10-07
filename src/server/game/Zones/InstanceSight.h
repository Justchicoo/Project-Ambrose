/*
 * Project Ambrose by Imjustchico
 * What can be seen in one zone instance at one update: the instance's objects and the wizards standing in it, filed in one grid by their global ids, with the objects exempt from area of interest, so each wizard's visibility set can be offered everything within reach of where it stands, itself left out, and every exempt object wherever it is.
 */

#ifndef AMBROSE_INSTANCESIGHT_H
#define AMBROSE_INSTANCESIGHT_H

#include "Grid.h"
#include "VisibilitySet.h"

#include <set>
#include <vector>

class Map;

class InstanceSight
{
public:
    static constexpr float WholeZoneCellSize = 4096.0f;

    InstanceSight(Map const& map, VisibilityRange range);

    void PlaceWizard(uint64 worldGuid, GridPoint const& at);
    bool IsObject(uint64 id) const { return _objects.contains(id); }
    VisibilityRange const& GetRange() const noexcept { return _range; }
    std::vector<VisibilityCandidate> CandidatesFor(uint64 viewer, GridPoint const& at) const;

private:
    VisibilityRange _range;
    Grid _places;
    std::set<uint64> _objects;
    std::set<uint64> _exempt;
};

#endif
