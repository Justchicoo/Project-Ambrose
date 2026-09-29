/*
 * Project Ambrose by Imjustchico
 * Plans a tick's meetings from what each wizard's session says of itself: departures come first, so a wizard that leaves and comes back in one tick is taken away before it is shown again, and an arrival meets the wizards already in its instance and the arrivals planned before it, seeing each that can be shown and seen by each when it can be shown itself, so a wizard whose own object could not be encoded still sees the others.
 */

#include "PlayerMeetings.h"

PlayerMeetingPlan PlanPlayerMeetings(std::vector<PlayerPresence> const& players)
{
    PlayerMeetingPlan plan;
    for (std::size_t left = 0; left < players.size(); ++left)
    {
        if (!players[left].LeftMapId)
            continue;
        for (std::size_t viewer = 0; viewer < players.size(); ++viewer)
            if (viewer != left && players[viewer].MapId == players[left].LeftMapId && players[viewer].WorldGuid != players[left].LeftGuid)
                plan.Hidings.push_back({ left, viewer });
    }
    for (std::size_t arrived = 0; arrived < players.size(); ++arrived)
    {
        PlayerPresence const& newcomer = players[arrived];
        if (!newcomer.Arrived || !newcomer.MapId)
            continue;
        for (std::size_t other = 0; other < players.size(); ++other)
        {
            PlayerPresence const& present = players[other];
            if (other == arrived || present.MapId != newcomer.MapId || present.WorldGuid == newcomer.WorldGuid || (present.Arrived && other > arrived))
                continue;
            if (present.Shown || newcomer.Shown)
                plan.Meetings.push_back({ arrived, other, present.Shown, newcomer.Shown });
        }
    }
    return plan;
}
