/*
 * Project Ambrose by Imjustchico
 * Tests one wizard's Dynamods: a new mod takes the next index after the placeholder, the same mod again changes nothing, a changed one replaces the old under a fresh index, a removal hides its tag, an add that shows its object shows it and one that only sets a state leaves it to its requirements while an untagged or unnamed object is left to its requirements, a stored set keeps the first of a twice-stored mod and counts on from the highest index it kept, a zone's armed mods leave out removals and other zones, and erasing a tag takes every zone's mod of it away and keeps its index from being given out again.
 */

#include "WizardDynaMods.h"

#include <gtest/gtest.h>

namespace
{
    constexpr std::string_view Hub = "WizardCity/WC_Hub";
}

TEST(WizardDynaModsTest, AChangedModReplacesTheOldUnderAFreshIndex)
{
    WizardDynaMods mods;
    DynaModChange const opened = mods.Apply(Hub, "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 9);
    ASSERT_TRUE(opened.Added);
    EXPECT_FALSE(opened.Removed);
    EXPECT_EQ(opened.Added->Index, 2) << "index 1 is the placeholder the full list opens with";
    EXPECT_FALSE(mods.Apply(Hub, "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 9).Changed());

    DynaModChange const closed = mods.Apply(Hub, "WC_GateCommons_ToUnicornWay", "IdleClosed", false, 9);
    ASSERT_TRUE(closed.Removed && closed.Added);
    EXPECT_EQ(closed.Removed->Index, 2);
    EXPECT_EQ(closed.Added->Index, 3);
    ASSERT_EQ(mods.All().size(), 1u);
    EXPECT_EQ(mods.Find(Hub, "WC_GateCommons_ToUnicornWay")->State, "IdleClosed");
    EXPECT_EQ(mods.Find("WizardCity/WC_Ravenwood", "WC_GateCommons_ToUnicornWay"), nullptr);
}

TEST(WizardDynaModsTest, ARemovalHidesItsTagAndAnAddShowsIt)
{
    WizardDynaMods mods;
    mods.Apply(Hub, "PetParkCollision", "", true, 9);
    mods.Apply(Hub, "Ultra_Rattlebones_40", "", false, 9);
    EXPECT_EQ(mods.Visibility(Hub, "PetParkCollision"), false);
    EXPECT_EQ(mods.Visibility(Hub, "Ultra_Rattlebones_40"), true);
    EXPECT_EQ(mods.Visibility(Hub, "WC_GateCommons_ToPetPark"), std::nullopt);
    EXPECT_EQ(mods.Visibility(Hub, ""), std::nullopt);

    mods.Apply(Hub, "AV-QuestLight-KnightsHall", "On", false, 9, false);
    EXPECT_EQ(mods.Visibility(Hub, "AV-QuestLight-KnightsHall"), std::nullopt) << "a mod that only sets a state leaves its objects to their requirements";
    DynaModChange const shown = mods.Apply(Hub, "AV-QuestLight-KnightsHall", "On", false, 9, true);
    EXPECT_TRUE(shown.Removed && shown.Added) << "showing its objects is a change of the mod";
    EXPECT_EQ(mods.Visibility(Hub, "AV-QuestLight-KnightsHall"), true);
}

TEST(WizardDynaModsTest, AStoredSetKeepsTheFirstOfATwiceStoredModAndCountsOnFromItsHighestIndex)
{
    WizardDynaMods mods({ { std::string(Hub), "PetParkGate", "IdleOpen", false, 7, 9 }, { std::string(Hub), "PetParkGate", "IdleClosed", false, 8, 9 },
        { std::string(Hub), "PetParkCollision", "", true, 3, 9 }, { "WizardCity/WC_Ravenwood", "Elsewhere", "", false, 2, 9 } });
    ASSERT_EQ(mods.All().size(), 3u);
    EXPECT_EQ(mods.Find(Hub, "PetParkGate")->State, "IdleOpen");
    EXPECT_EQ(mods.Apply(Hub, "Ultra_Rattlebones_40", "", false, 9).Added->Index, 8) << "it counts on from the highest index it kept";

    std::vector<DynaMod> const armed = mods.Armed(Hub);
    ASSERT_EQ(armed.size(), 2u);
    EXPECT_EQ(armed[0].Tag, "PetParkGate");
    EXPECT_EQ(armed[1].Tag, "Ultra_Rattlebones_40");
}

TEST(WizardDynaModsTest, ErasingATagTakesEveryZonesModOfItAwayAndKeepsItsIndexSpent)
{
    WizardDynaMods mods;
    mods.Apply(Hub, "AV-QuestLight-KnightsHall", "On", false, 9);
    mods.Apply("Avalon/AV_Z00_Hub", "AV-QuestLight-KnightsHall", "On", false, 9);
    mods.Apply(Hub, "PetParkCollision", "", true, 9);
    std::vector<DynaMod> const erased = mods.Erase("AV-QuestLight-KnightsHall");
    ASSERT_EQ(erased.size(), 2u);
    EXPECT_EQ(erased[0].Zone, Hub);
    EXPECT_EQ(erased[1].Zone, "Avalon/AV_Z00_Hub");
    EXPECT_EQ(erased[1].Index, 3);
    ASSERT_EQ(mods.All().size(), 1u);
    EXPECT_EQ(mods.Find(Hub, "AV-QuestLight-KnightsHall"), nullptr);
    EXPECT_EQ(mods.Visibility(Hub, "AV-QuestLight-KnightsHall"), std::nullopt) << "with no mod left, the object's requirements decide";
    EXPECT_TRUE(mods.Erase("AV-QuestLight-KnightsHall").empty()) << "a tag with no mod erases nothing";
    EXPECT_EQ(mods.Apply(Hub, "AV-QuestLight-KnightsHall", "On", false, 9).Added->Index, 5) << "an erased mod's index is not given out again";
}
