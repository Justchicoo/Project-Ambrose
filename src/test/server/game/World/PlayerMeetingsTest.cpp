/*
 * Project Ambrose by Imjustchico
 * Tests who the world shows to whom in a tick: a newcomer and every wizard already in its instance see each other and no wizard of another instance, two newcomers of one tick meet once, a wizard whose object could not be encoded still sees the others without being shown, a wizard that left is taken away from every wizard still in its instance and from no one else, and a quick relog is taken away from the others before it is shown again, and never from or to its own new session.
 */

#include "PlayerMeetings.h"

#include <gtest/gtest.h>

namespace
{
    uint64 nextGuid = 100;

    PlayerPresence Present(uint32 map, bool shown = true, uint64 guid = 0)
    {
        PlayerPresence presence;
        presence.MapId = map;
        presence.WorldGuid = guid != 0 ? guid : ++nextGuid;
        presence.Shown = shown;
        return presence;
    }

    PlayerPresence Arriving(uint32 map, bool shown = true, uint64 guid = 0)
    {
        PlayerPresence presence = Present(map, shown, guid);
        presence.Arrived = true;
        return presence;
    }

    PlayerPresence Leaving(uint32 map, uint64 guid)
    {
        PlayerPresence presence;
        presence.WorldGuid = guid;
        presence.LeftMapId = map;
        presence.LeftGuid = guid;
        return presence;
    }
}

TEST(PlayerMeetingsTest, ANewcomerAndEveryWizardInItsInstanceSeeEachOtherAndNoOneElse)
{
    PlayerMeetingPlan const plan = PlanPlayerMeetings({ Present(7), Present(8), Arriving(7), Present(7) });
    EXPECT_TRUE(plan.Hidings.empty());
    EXPECT_EQ(plan.Meetings, (std::vector<PlayerMeeting>{ { 2, 0, true, true }, { 2, 3, true, true } }));
    EXPECT_TRUE(PlanPlayerMeetings({ Present(7), Present(7) }).Meetings.empty()) << "wizards already in an instance have met";
}

TEST(PlayerMeetingsTest, TwoNewcomersOfOneTickMeetOnce)
{
    PlayerMeetingPlan const plan = PlanPlayerMeetings({ Arriving(7), Present(7), Arriving(7) });
    EXPECT_EQ(plan.Meetings, (std::vector<PlayerMeeting>{ { 0, 1, true, true }, { 2, 0, true, true }, { 2, 1, true, true } }));
}

TEST(PlayerMeetingsTest, AWizardThatCannotBeShownStillSeesTheOthers)
{
    PlayerMeetingPlan const plan = PlanPlayerMeetings({ Present(7), Arriving(7, false), Present(7, false) });
    EXPECT_EQ(plan.Meetings, (std::vector<PlayerMeeting>{ { 1, 0, true, false } })) << "two wizards neither can be shown have nothing to meet over";
    PlayerMeetingPlan const seen = PlanPlayerMeetings({ Present(7, false), Arriving(7) });
    EXPECT_EQ(seen.Meetings, (std::vector<PlayerMeeting>{ { 1, 0, false, true } }));
    PlayerMeetingPlan const earlier = PlanPlayerMeetings({ Arriving(7, false), Arriving(7) });
    EXPECT_EQ(earlier.Meetings, (std::vector<PlayerMeeting>{ { 1, 0, false, true } })) << "a later newcomer is still seen by an earlier one that cannot be shown";
}

TEST(PlayerMeetingsTest, AWizardThatLeftIsTakenAwayFromTheWizardsStillInItsInstance)
{
    PlayerMeetingPlan const plan = PlanPlayerMeetings({ Present(7), Leaving(7, 55), Present(8), Present(7, false), Leaving(9, 56) });
    EXPECT_EQ(plan.Hidings, (std::vector<PlayerHiding>{ { 1, 0 }, { 1, 3 } }));
    EXPECT_TRUE(plan.Meetings.empty());
}

TEST(PlayerMeetingsTest, AQuickRelogIsTakenAwayFromTheOthersBeforeItIsShownAgainAndNeverFromItself)
{
    PlayerMeetingPlan const plan = PlanPlayerMeetings({ Present(7), Leaving(7, 55), Arriving(7, true, 55) });
    EXPECT_EQ(plan.Hidings, (std::vector<PlayerHiding>{ { 1, 0 } })) << "the wizard's new session is not told to take its own wizard away";
    EXPECT_EQ(plan.Meetings, (std::vector<PlayerMeeting>{ { 2, 0, true, true } }));
    PlayerMeetingPlan const twice = PlanPlayerMeetings({ Present(7, true, 55), Arriving(7, true, 55) });
    EXPECT_TRUE(twice.Meetings.empty()) << "a session still holding the same wizard is not shown it as another";
}
