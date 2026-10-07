/*
 * Project Ambrose by Imjustchico
 * Tests a wizard's backpack: its capacity is the slots every backpack has plus the extra slots, never below zero; an add to a full backpack is refused without spending an item id or giving a row to store, and room made by more slots lets the next add through; added items take ids from the item line and slots in arrival order, which a backpack read from rows in any order keeps; and an item is trashed only when the backpack holds it, of the template named, and unlocked.
 */

#include "ObjectGuid.h"
#include "PlayerBackpack.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    CharacterItem Stored(uint64 guid, uint32 templateId, uint32 slot, bool locked = false)
    {
        CharacterItem item;
        item.Guid = guid;
        item.TemplateId = templateId;
        item.Slot = slot;
        item.Locked = locked;
        return item;
    }
}

TEST(PlayerBackpackTest, CapacityIsTheSlotsPlusTheExtraSlots)
{
    EXPECT_EQ(PlayerBackpack::CapacityFor(80, 0), 80u);
    EXPECT_EQ(PlayerBackpack::CapacityFor(80, 20), 100u);
    EXPECT_EQ(PlayerBackpack::CapacityFor(-5, 3), 3u);
    EXPECT_EQ(PlayerBackpack::CapacityFor(0, 0), 0u);
}

TEST(PlayerBackpackTest, AnAddToAFullBackpackSpendsNoIdAndGivesNothingToStore)
{
    GuidGenerator guids(ObjectGuid::ItemBase);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ Stored(ObjectGuid::ItemBase + 50, 7, 0) });
    BackpackAdd const refused = backpack.Add(9, 1, 1, guids, 1000);
    EXPECT_EQ(refused.Result, BackpackAddResult::Full);
    EXPECT_FALSE(refused.Item);
    EXPECT_EQ(backpack.Size(), 1u);
    EXPECT_EQ(guids.PeekNext(), ObjectGuid::ItemBase);

    BackpackAdd const added = backpack.Add(9, 1, 2, guids, 1000);
    ASSERT_EQ(added.Result, BackpackAddResult::Added);
    ASSERT_TRUE(added.Item);
    EXPECT_EQ(added.Item->Guid, ObjectGuid::ItemBase);
    EXPECT_EQ(added.Item->TemplateId, 9u);
    EXPECT_EQ(added.Item->Slot, 1u);
    EXPECT_EQ(added.Item->Created, 1000u);
    EXPECT_TRUE(ObjectGuid::IsItem(added.Item->Guid));
    EXPECT_FALSE(ObjectGuid::IsRuntime(added.Item->Guid));
    EXPECT_EQ(backpack.Size(), 2u);
}

TEST(PlayerBackpackTest, ABackpackReadFromRowsKeepsTheirArrivalOrder)
{
    PlayerBackpack backpack = PlayerBackpack::FromStored({ Stored(30, 3, 4), Stored(10, 1, 0), Stored(20, 2, 2) });
    ASSERT_EQ(backpack.Size(), 3u);
    EXPECT_EQ(backpack.GetItems()[0].Guid, 10u);
    EXPECT_EQ(backpack.GetItems()[1].Guid, 20u);
    EXPECT_EQ(backpack.GetItems()[2].Guid, 30u);
    GuidGenerator guids(ObjectGuid::ItemBase);
    BackpackAdd const added = backpack.Add(4, 1, 10, guids, 0);
    ASSERT_TRUE(added.Item);
    EXPECT_EQ(added.Item->Slot, 5u);
}

TEST(PlayerBackpackTest, AnItemIsTrashedOnlyWhenHeldOfTheNamedTemplateAndUnlocked)
{
    PlayerBackpack backpack = PlayerBackpack::FromStored({ Stored(10, 1, 0), Stored(11, 2, 1, true) });
    EXPECT_EQ(backpack.CanTrash(99, 1), BackpackTrashResult::NotOwned);
    EXPECT_EQ(backpack.CanTrash(10, 2), BackpackTrashResult::WrongTemplate);
    EXPECT_EQ(backpack.CanTrash(11, 2), BackpackTrashResult::Locked);
    EXPECT_EQ(backpack.CanTrash(10, 1), BackpackTrashResult::Trashed);
    EXPECT_EQ(backpack.CanTrash(10, 0), BackpackTrashResult::Trashed);
    EXPECT_FALSE(backpack.Remove(99));
    ASSERT_TRUE(backpack.Remove(10));
    EXPECT_EQ(backpack.Find(10), nullptr);
    EXPECT_EQ(backpack.Size(), 1u);
}
