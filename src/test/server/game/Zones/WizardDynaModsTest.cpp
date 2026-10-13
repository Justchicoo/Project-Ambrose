/*
 * Project Ambrose by Imjustchico
 * Tests one wizard's Dynamods: a new mod takes the next index, the same mod again changes nothing, a changed one replaces the old under a fresh index, a removal hides its tag and an add shows it while an untagged or unnamed object is left to its requirements, a stored set keeps the first of a twice-stored mod and counts on from the highest index it kept, and a zone's armed mods leave out removals and other zones.
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
    EXPECT_EQ(opened.Added->Index, 1);
    EXPECT_FALSE(mods.Apply(Hub, "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 9).Changed());

    DynaModChange const closed = mods.Apply(Hub, "WC_GateCommons_ToUnicornWay", "IdleClosed", false, 9);
    ASSERT_TRUE(closed.Removed && closed.Added);
    EXPECT_EQ(closed.Removed->Index, 1);
    EXPECT_EQ(closed.Added->Index, 2);
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
