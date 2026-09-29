/*
 * Project Ambrose by Imjustchico
 * Who is shown to whom in a tick: a wizard that left an instance is taken away from every wizard still in it, and a wizard that arrived sees every wizard in its instance that can be shown and is seen by them when it can be shown itself, with two wizards that arrive in the same tick meeting once, not twice, and a wizard never taken away from or shown to a session that holds the same wizard, as a quick relog does.
 */

#ifndef AMBROSE_PLAYERMEETINGS_H
#define AMBROSE_PLAYERMEETINGS_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <vector>

struct PlayerPresence
{
    std::optional<uint32> MapId;
    uint64 WorldGuid = 0;
    bool Shown = false;
    bool Arrived = false;
    std::optional<uint32> LeftMapId;
    uint64 LeftGuid = 0;
};

struct PlayerMeeting
{
    std::size_t Arrived = 0;
    std::size_t Other = 0;
    bool ArrivedSees = false;
    bool OtherSees = false;
    bool operator==(PlayerMeeting const&) const = default;
};

struct PlayerHiding
{
    std::size_t Left = 0;
    std::size_t Viewer = 0;
    bool operator==(PlayerHiding const&) const = default;
};

struct PlayerMeetingPlan
{
    std::vector<PlayerHiding> Hidings;
    std::vector<PlayerMeeting> Meetings;
};

PlayerMeetingPlan PlanPlayerMeetings(std::vector<PlayerPresence> const& players);

#endif
