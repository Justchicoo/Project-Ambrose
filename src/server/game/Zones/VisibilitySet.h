/*
 * Project Ambrose by Imjustchico
 * What one wizard is shown of its zone instance: an object comes into view within the visibility distance and leaves it only past that distance and the hysteresis band beyond it, so one standing at the edge is neither shown nor taken away over and over; the first time an object is shown it is new to the client, which MSG_NEWOBJECT tells, every later time it is added back, which MSG_ADDOBJECT tells, and leaving view is MSG_REMOVEOBJECT; an object exempt from area of interest is always shown, and a distance of zero or less shows everything. The range is read at each update, so a changed setting applies at the next one.
 */

#ifndef AMBROSE_VISIBILITYSET_H
#define AMBROSE_VISIBILITYSET_H

#include "Grid.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <set>
#include <vector>

struct VisibilityRange
{
    float Distance = 0.0f;
    float Hysteresis = 0.0f;

    bool ShowsEverything() const noexcept { return !(Distance > 0.0f); }
    float Reach() const noexcept { return ShowsEverything() ? 0.0f : Distance + (Hysteresis > 0.0f ? Hysteresis : 0.0f); }

    static VisibilityRange Resolve(float distanceSetting, float hysteresisSetting, std::optional<float> zoneFarClip) noexcept;
};

struct VisibilityCandidate
{
    uint64 Id = 0;
    float DistanceSquared = 0.0f;
    bool Exempt = false;
};

struct VisibilityChanges
{
    std::vector<uint64> New;
    std::vector<uint64> Added;
    std::vector<uint64> Removed;

    bool Empty() const noexcept { return New.empty() && Added.empty() && Removed.empty(); }
};

class VisibilitySet
{
public:
    VisibilityChanges Update(std::vector<VisibilityCandidate> const& nearby, VisibilityRange const& range);
    VisibilityChanges Update(Grid const& grid, uint64 viewer, std::vector<uint64> const& exempt, VisibilityRange const& range);

    bool Forget(uint64 id);
    void Clear();

    bool IsVisible(uint64 id) const { return _visible.contains(id); }
    bool IsKnown(uint64 id) const { return _known.contains(id); }
    std::size_t VisibleCount() const noexcept { return _visible.size(); }

private:
    std::set<uint64> _known;
    std::set<uint64> _visible;
};

#endif
