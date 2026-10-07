/*
 * Project Ambrose by Imjustchico
 * Tests what one instance offers a wizard to be shown: the objects and wizards within reach of where it stands and never itself, every object exempt from area of interest however far away, and everything when the range shows the whole zone; and that a wizard who walks out of reach and back is taken away and then shown again, which the session sends as a whole new object because the r806919 client ignores MSG_ADDOBJECT.
 */

#include "InstanceSight.h"
#include "Map.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{
    void Add(Map& map, uint64 id, float x, float y, bool exempt = false)
    {
        MapObject object;
        object.GlobalId = id;
        object.Spawn.Position = { x, y, 0.0f };
        object.ExemptFromAoi = exempt;
        map.AddObject(std::move(object));
    }

    std::vector<uint64> Ids(std::vector<VisibilityCandidate> const& candidates)
    {
        std::vector<uint64> ids;
        for (VisibilityCandidate const& candidate : candidates)
            ids.push_back(candidate.Id);
        std::sort(ids.begin(), ids.end());
        return ids;
    }
}

TEST(InstanceSightTest, AWizardIsOfferedWhatIsWithinReachEveryExemptObjectAndNeverItself)
{
    Map map(1, "WizardCity/WC_Hub", true);
    Add(map, 10, 50.0f, 0.0f);
    Add(map, 11, 5000.0f, 0.0f);
    Add(map, 12, 9000.0f, 9000.0f, true);
    InstanceSight sight(map, VisibilityRange::Resolve(100.0f, 20.0f, std::nullopt));
    sight.PlaceWizard(1, { 0.0f, 0.0f, 0.0f });
    sight.PlaceWizard(2, { 0.0f, 110.0f, 0.0f });
    sight.PlaceWizard(3, { 0.0f, 900.0f, 0.0f });

    std::vector<VisibilityCandidate> const offered = sight.CandidatesFor(1, { 0.0f, 0.0f, 0.0f });
    EXPECT_EQ(Ids(offered), (std::vector<uint64>{ 2, 10, 12 })) << "the far object and the far wizard are out of reach, the exempt landmark is not";
    auto const landmark = std::find_if(offered.begin(), offered.end(), [](VisibilityCandidate const& candidate) { return candidate.Id == 12; });
    ASSERT_NE(landmark, offered.end());
    EXPECT_TRUE(landmark->Exempt);
    EXPECT_TRUE(sight.IsObject(10));
    EXPECT_FALSE(sight.IsObject(2));

    InstanceSight whole(map, VisibilityRange::Resolve(0.0f, 20.0f, std::nullopt));
    whole.PlaceWizard(1, { 0.0f, 0.0f, 0.0f });
    whole.PlaceWizard(3, { 0.0f, 900.0f, 0.0f });
    EXPECT_EQ(Ids(whole.CandidatesFor(1, { 0.0f, 0.0f, 0.0f })), (std::vector<uint64>{ 3, 10, 11, 12 })) << "no distance and no far clip shows the whole zone";
}

TEST(InstanceSightTest, AWizardWhoWalksOutOfReachAndBackIsTakenAwayAndShownAgain)
{
    Map map(1, "WizardCity/WC_Hub", true);
    VisibilityRange const range = VisibilityRange::Resolve(100.0f, 20.0f, std::nullopt);
    VisibilitySet seen;
    auto const step = [&](float otherY)
    {
        InstanceSight sight(map, range);
        sight.PlaceWizard(1, { 0.0f, 0.0f, 0.0f });
        sight.PlaceWizard(2, { 0.0f, otherY, 0.0f });
        return seen.Update(sight.CandidatesFor(1, { 0.0f, 0.0f, 0.0f }), range);
    };
    EXPECT_EQ(step(50.0f).New, (std::vector<uint64>{ 2 }));
    EXPECT_TRUE(step(115.0f).Empty()) << "inside the hysteresis band nothing changes";
    EXPECT_EQ(step(400.0f).Removed, (std::vector<uint64>{ 2 }));
    VisibilityChanges const back = step(60.0f);
    EXPECT_TRUE(back.New.empty());
    EXPECT_EQ(back.Added, (std::vector<uint64>{ 2 })) << "shown again; the session sends it as MSG_NEWOBJECT";
}
