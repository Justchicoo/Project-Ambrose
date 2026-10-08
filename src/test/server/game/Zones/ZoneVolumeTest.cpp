/*
 * Project Ambrose by Imjustchico
 * Checks volume containment for each shape, on and just past each boundary, a wizard placed inside counting as inside without an enter, presence that leaves only past the exit margin, and triggers that fire on their events, once per wizard per cooldown, no more often than their trigger max and never while they carry requirements.
 */

#include "ZoneTriggers.h"
#include "ZoneVolume.h"

#include <gtest/gtest.h>

namespace
{
    ZoneVolume Volume(VolumeShape shape)
    {
        ZoneVolume volume;
        volume.Shape = shape;
        volume.X = 100.0f;
        volume.Y = 200.0f;
        volume.Z = 10.0f;
        volume.Radius = 50.0f;
        volume.Length = 40.0f;
        volume.Width = 60.0f;
        volume.Depth = 80.0f;
        return volume;
    }
}

TEST(ZoneVolumeTest, EachShapeContainsItsBoundaryAndNothingPastIt)
{
    EXPECT_EQ(ZoneVolume::ShapeOf("SPHERE"), VolumeShape::Sphere);
    EXPECT_EQ(ZoneVolume::ShapeOf("Cylinder"), VolumeShape::Cylinder);
    EXPECT_EQ(ZoneVolume::ShapeOf("box"), VolumeShape::Box);
    EXPECT_EQ(ZoneVolume::ShapeOf("cone"), VolumeShape::Unknown);

    ZoneVolume const sphere = Volume(VolumeShape::Sphere);
    EXPECT_TRUE(sphere.Contains(150.0f, 200.0f, 10.0f));
    EXPECT_FALSE(sphere.Contains(150.5f, 200.0f, 10.0f));
    EXPECT_FALSE(sphere.Contains(100.0f, 200.0f, 60.5f));

    ZoneVolume const cylinder = Volume(VolumeShape::Cylinder);
    EXPECT_TRUE(cylinder.Contains(100.0f, 250.0f, 30.0f));
    EXPECT_FALSE(cylinder.Contains(100.0f, 250.5f, 10.0f));
    EXPECT_FALSE(cylinder.Contains(100.0f, 200.0f, 30.5f));

    ZoneVolume const box = Volume(VolumeShape::Box);
    EXPECT_TRUE(box.Contains(130.0f, 240.0f, 30.0f));
    EXPECT_FALSE(box.Contains(130.5f, 200.0f, 10.0f));
    EXPECT_FALSE(box.Contains(100.0f, 240.5f, 10.0f));
    EXPECT_FALSE(box.Contains(100.0f, 200.0f, -10.5f));

    EXPECT_FALSE(Volume(VolumeShape::Unknown).Contains(100.0f, 200.0f, 10.0f));
}

TEST(ZoneVolumeTest, PresenceEntersAtTheEdgeLeavesPastTheMarginAndAPlacedWizardIsAlreadyInside)
{
    ZoneVolume const sphere = Volume(VolumeShape::Sphere);
    VolumePresence walker;
    EXPECT_EQ(walker.Update(sphere, 160.0f, 200.0f, 10.0f), VolumePresence::Change::None);
    EXPECT_EQ(walker.Update(sphere, 150.0f, 200.0f, 10.0f), VolumePresence::Change::Entered);
    EXPECT_EQ(walker.Update(sphere, 151.0f, 200.0f, 10.0f), VolumePresence::Change::None) << "within the exit margin it stays inside";
    EXPECT_EQ(walker.Update(sphere, 150.0f, 200.0f, 10.0f), VolumePresence::Change::None);
    EXPECT_EQ(walker.Update(sphere, 152.5f, 200.0f, 10.0f), VolumePresence::Change::Exited);

    VolumePresence spawned;
    spawned.Place(sphere, 100.0f, 200.0f, 10.0f);
    EXPECT_TRUE(spawned.Inside());
    EXPECT_EQ(spawned.Update(sphere, 101.0f, 200.0f, 10.0f), VolumePresence::Change::None) << "a wizard placed inside fires no enter";
}

TEST(ZoneVolumeTest, ATriggerFiresOncePerWizardPerCooldownWithinItsMaxAndNeverWithRequirements)
{
    ZoneTriggers triggers({ { 1, "Trigger POI Ravenwood", -1, 30.0f, false, { "Enter_Ravenwood POI" }, {} }, { 2, "Once", 1, 0.0f, false, { "Enter_Ravenwood POI" }, {} },
        { 3, "Needs a quest", -1, 0.0f, true, { "Enter_Ravenwood POI" }, {} }, { 4, "Elsewhere", -1, 0.0f, false, { "Enter_Other" }, {} } });
    ZoneTriggers::Clock::time_point const start{};

    std::vector<ZoneTrigger const*> fired = triggers.Post("Enter_Ravenwood POI", 7, start);
    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0]->Name, "Trigger POI Ravenwood");
    EXPECT_EQ(fired[1]->Name, "Once");

    EXPECT_TRUE(triggers.Post("Enter_Ravenwood POI", 7, start + std::chrono::seconds(10)).empty()) << "within the cooldown and past its max";
    fired = triggers.Post("Enter_Ravenwood POI", 8, start + std::chrono::seconds(10));
    ASSERT_EQ(fired.size(), 1u) << "another wizard has its own cooldown";
    EXPECT_EQ(fired[0]->Name, "Trigger POI Ravenwood");
    fired = triggers.Post("Enter_Ravenwood POI", 7, start + std::chrono::seconds(31));
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->Name, "Trigger POI Ravenwood");
}
