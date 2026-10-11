/*
 * Project Ambrose by Imjustchico
 * Tests the bank's storage: a store read from rows keeps their arrival order, an item is added only while the store has room and takes the lowest free slot, which a withdrawal frees for the next arrival, and a store of a full bank refuses an add without changing anything, an item found by its id and a slot freed by a taken item resting for the next one.
 */

#include "BankStore.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    CharacterItem Stored(uint64 guid, uint32 templateId, uint32 slot)
    {
        CharacterItem item;
        item.Guid = guid;
        item.TemplateId = templateId;
        item.Slot = slot;
        return item;
    }
}

TEST(BankStoreTest, AStoreReadFromRowsKeepsTheirArrivalOrder)
{
    BankStore store = BankStore::FromStored({ Stored(30, 3, 4), Stored(10, 1, 0), Stored(20, 2, 2) });
    ASSERT_EQ(store.Size(), 3u);
    EXPECT_EQ(store.GetItems()[0].Guid, 30u);
    EXPECT_EQ(store.GetItems()[1].Guid, 10u);
    EXPECT_EQ(store.GetItems()[2].Guid, 20u);
    EXPECT_EQ(store.GetItems()[0].Slot, 4u);
    EXPECT_EQ(store.GetItems()[1].Slot, 0u);
    EXPECT_EQ(store.GetItems()[2].Slot, 2u);
}

TEST(BankStoreTest, AnItemIsAddedOnlyWhileTheStoreHasRoomAndTakesTheLowestFreeSlot)
{
    BankStore store = BankStore::FromStored({ Stored(10, 1, 0), Stored(20, 2, 2) });
    EXPECT_EQ(store.CanAdd(2), false);
    EXPECT_EQ(store.CanAdd(3), true);
    EXPECT_FALSE(store.FreeSlot(2));
    CharacterItem const full = store.Put(Stored(40, 4, 9), 2);
    EXPECT_EQ(store.Size(), 2u) << "a full store keeps the item out";
    EXPECT_EQ(full.Slot, 9u) << "the refused item is given back as it arrived";
    EXPECT_FALSE(store.Holds(40u));

    CharacterItem const added = store.Put(Stored(40, 4, 9), 3);
    ASSERT_TRUE(store.Holds(40u));
    EXPECT_EQ(added.Slot, 1u) << "slot 1 is the lowest free one under slots 0 and 2";
    EXPECT_EQ(store.Size(), 3u);
}

TEST(BankStoreTest, AWithdrawalFreesItsSlotForTheNextArrival)
{
    BankStore store = BankStore::FromStored({ Stored(10, 1, 0), Stored(20, 2, 1) });
    std::optional<CharacterItem> const taken = store.Remove(10u);
    ASSERT_TRUE(taken);
    EXPECT_EQ(taken->Guid, 10u);
    EXPECT_EQ(taken->TemplateId, 1u);
    EXPECT_EQ(store.Size(), 1u);
    EXPECT_FALSE(store.Holds(10u));

    CharacterItem const added = store.Put(Stored(30, 3, 7), 2);
    EXPECT_EQ(added.Slot, 0u) << "the slot the withdrawal freed is the next arrival's";
}

TEST(BankStoreTest, FindAnswersTheItemItHoldsAndNothingForOneItDoesNot)
{
    BankStore store = BankStore::FromStored({ Stored(10, 1, 0) });
    CharacterItem const* const found = store.Find(10u);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Slot, 0u);
    EXPECT_EQ(store.Find(99u), nullptr);
    EXPECT_EQ(store.Find(0u), nullptr);
    std::optional<CharacterItem> const nothing = store.Remove(99u);
    EXPECT_FALSE(nothing);
    EXPECT_EQ(store.Size(), 1u) << "a removal of an item the store does not hold changes nothing";
}
