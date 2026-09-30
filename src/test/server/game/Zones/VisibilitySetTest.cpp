/*
 * Project Ambrose by Imjustchico
 * Tests what a wizard is shown: an object crossing the visibility distance back and forth inside the hysteresis band changes nothing, one that comes back after leaving is added back rather than made new a second time, lowering the distance takes away at the next update what is now out of range with no restart, an exempt object stays shown however far it is, a distance of zero takes the zone's far clip and a zone with none shows everything, and the grid-driven update never shows a wizard itself.
 */

#include "VisibilitySet.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    VisibilityCandidate At(uint64 id, float distance, bool exempt = false)
    {
        return { id, distance * distance, exempt };
    }

    VisibilityRange Range(float distance, float hysteresis)
    {
        return VisibilityRange::Resolve(distance, hysteresis, std::nullopt);
    }
}

TEST(VisibilitySetTest, CrossingTheBoundaryBackAndForthInsideTheHysteresisBandSendsNothing)
{
    VisibilitySet set;
    VisibilityRange const range = Range(100.0f, 20.0f);
    VisibilityChanges const shown = set.Update({ At(9, 90.0f) }, range);
    ASSERT_EQ(shown.New, (std::vector<uint64>{ 9 }));
    for (float const distance : { 101.0f, 99.0f, 119.0f, 100.0f, 115.0f, 120.0f })
        EXPECT_TRUE(set.Update({ At(9, distance) }, range).Empty()) << distance;
    EXPECT_TRUE(set.IsVisible(9));

    VisibilityChanges const gone = set.Update({ At(9, 121.0f) }, range);
    EXPECT_EQ(gone.Removed, (std::vector<uint64>{ 9 }));
    for (float const distance : { 110.0f, 101.0f, 119.0f })
        EXPECT_TRUE(set.Update({ At(9, distance) }, range).Empty()) << "outside the distance it stays away: " << distance;
    EXPECT_FALSE(set.IsVisible(9));
}

TEST(VisibilitySetTest, ReEntryAfterExitIsAnAddNotASecondNewObject)
{
    VisibilitySet set;
    VisibilityRange const range = Range(50.0f, 5.0f);
    EXPECT_EQ(set.Update({ At(4, 10.0f) }, range).New, (std::vector<uint64>{ 4 }));
    EXPECT_EQ(set.Update({}, range).Removed, (std::vector<uint64>{ 4 })) << "an object no longer near at all leaves view";
    VisibilityChanges const back = set.Update({ At(4, 20.0f) }, range);
    EXPECT_TRUE(back.New.empty());
    EXPECT_EQ(back.Added, (std::vector<uint64>{ 4 }));
    EXPECT_TRUE(set.IsKnown(4));

    EXPECT_TRUE(set.Forget(4));
    EXPECT_EQ(set.Update({ At(4, 20.0f) }, range).New, (std::vector<uint64>{ 4 })) << "a forgotten object is new again, as one the client destroyed";
}

TEST(VisibilitySetTest, LoweringTheDistanceTakesAwayWhatIsNowOutOfRangeOnTheNextUpdate)
{
    VisibilitySet set;
    std::vector<VisibilityCandidate> const nearby = { At(1, 30.0f), At(2, 150.0f), At(3, 250.0f), At(4, 1000.0f, true) };
    VisibilityChanges const first = set.Update(nearby, Range(300.0f, 10.0f));
    EXPECT_EQ(first.New, (std::vector<uint64>{ 1, 2, 3, 4 }));
    VisibilityChanges const lowered = set.Update(nearby, Range(100.0f, 10.0f));
    EXPECT_EQ(lowered.Removed, (std::vector<uint64>{ 2, 3 }));
    EXPECT_TRUE(lowered.New.empty() && lowered.Added.empty());
    EXPECT_TRUE(set.IsVisible(1));
    EXPECT_TRUE(set.IsVisible(4)) << "an exempt object stays shown however far it is";
    EXPECT_EQ(set.Update(nearby, Range(300.0f, 10.0f)).Added, (std::vector<uint64>{ 2, 3 }));
}

TEST(VisibilitySetTest, ADistanceOfZeroTakesTheZonesFarClipAndAZoneWithNoneShowsEverything)
{
    VisibilityRange const fromZone = VisibilityRange::Resolve(0.0f, 20.0f, 800.0f);
    EXPECT_FLOAT_EQ(fromZone.Distance, 800.0f);
    EXPECT_FLOAT_EQ(fromZone.Reach(), 820.0f);
    EXPECT_FLOAT_EQ(VisibilityRange::Resolve(300.0f, 20.0f, 800.0f).Distance, 300.0f) << "a setting above zero wins over the zone";
    VisibilityRange const everything = VisibilityRange::Resolve(0.0f, 20.0f, std::nullopt);
    EXPECT_TRUE(everything.ShowsEverything());

    VisibilitySet set;
    EXPECT_EQ(set.Update({ At(1, 1.0e6f), At(2, 5.0f) }, everything).New, (std::vector<uint64>{ 1, 2 }));
}

TEST(VisibilitySetTest, TheGridUpdateShowsNearbyAndExemptObjectsButNeverTheWizardItself)
{
    Grid grid(64.0f);
    grid.Place(100, { 0.0f, 0.0f, 0.0f });
    grid.Place(1, { 30.0f, 0.0f, 0.0f });
    grid.Place(2, { 500.0f, 0.0f, 0.0f });
    grid.Place(3, { 5000.0f, 5000.0f, 0.0f });

    VisibilitySet set;
    VisibilityRange const range = Range(100.0f, 10.0f);
    VisibilityChanges const shown = set.Update(grid, 100, { 3, 100, 77 }, range);
    EXPECT_EQ(shown.New, (std::vector<uint64>{ 1, 3 })) << "77 is not in the instance, and 100 is the wizard";

    grid.Place(1, { 105.0f, 0.0f, 0.0f });
    EXPECT_TRUE(set.Update(grid, 100, { 3 }, range).Empty());
    grid.Place(2, { 90.0f, 0.0f, 0.0f });
    grid.Place(1, { 111.0f, 0.0f, 0.0f });
    VisibilityChanges const moved = set.Update(grid, 100, { 3 }, range);
    EXPECT_EQ(moved.New, (std::vector<uint64>{ 2 }));
    EXPECT_EQ(moved.Removed, (std::vector<uint64>{ 1 }));
}
