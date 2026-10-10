/*
 * Project Ambrose by Imjustchico
 * Tests what a wizard wears against slots and item templates the test writes itself: a Fire-only robe on an Ice wizard is refused and stays in the backpack, a robe below the wizard's level requirement is refused, equipping into an occupied slot moves the old item back to the backpack and a backpack already over its capacity refuses that swap, the weapon slot takes a wand through its OR adjectives while a hat fits no robe slot, an item not in the backpack or a slot that does not exist is refused, a slot holding several items gives back its oldest only once it is full, and unequipping returns the item to the backpack unless the backpack is full.
 */

#include "EquipmentSlots.h"
#include "ObjectGuid.h"
#include "PlayerEquipment.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr uint32 FireRobe = 101;
    constexpr uint32 IceRobe = 102;
    constexpr uint32 StarterRobe = 103;
    constexpr uint32 Hat = 104;
    constexpr uint32 Wand = 105;
    constexpr uint32 HighLevelRobe = 106;
    constexpr uint32 Elixir = 107;
    constexpr uint32 Roomy = 1000;

    EquipSlot Slot(std::string name, std::vector<std::string> all, std::vector<std::string> any = {}, uint32 maxItems = 1)
    {
        EquipSlot slot;
        slot.Name = std::move(name);
        slot.AdjectivesAnd = std::move(all);
        slot.AdjectivesOr = std::move(any);
        slot.MaxItems = maxItems;
        return slot;
    }

    EquipmentSlots Slots()
    {
        return EquipmentSlots::FromSlots({ Slot("Hat", { "Hat" }), Slot("Weapon", {}, { "Weapon", "Wand", "Staff", "Rod" }), Slot("Robe", { "Robe" }),
            Slot("Elixir", { "Elixir" }, {}, 2) });
    }

    ItemTemplatePart Requirement(std::string className, std::string school, std::optional<double> value = std::nullopt, std::optional<int64> comparison = std::nullopt)
    {
        ItemTemplatePart part;
        part.ClassName = std::move(className);
        part.MagicSchool = std::move(school);
        part.NumericValue = value;
        part.OperatorType = comparison;
        return part;
    }

    ItemTemplateRecord Item(uint32 id, std::vector<std::string> adjectives, std::vector<ItemTemplatePart> requirements = {})
    {
        ItemTemplateRecord item;
        item.TemplateId = id;
        item.ClassName = std::string(ItemTemplateRecord::ItemClass);
        item.ObjectName = "Item " + std::to_string(id);
        item.Adjectives = std::move(adjectives);
        if (!requirements.empty())
            item.EquipRequirements = ItemRequirementList{ false, 0, std::move(requirements) };
        return item;
    }

    std::shared_ptr<ItemTemplateStore const> Templates()
    {
        return ItemTemplateStore::Build({ Item(FireRobe, { "Robe" }, { Requirement("class ReqSchoolOfFocus", "Fire") }),
            Item(IceRobe, { "Robe" }, { Requirement("class ReqSchoolOfFocus", "Ice") }), Item(StarterRobe, { "Robe" }), Item(Hat, { "Hat" }), Item(Wand, { "Wand" }),
            Item(HighLevelRobe, { "Robe" }, { Requirement("class ReqSchoolOfFocus", "Ice"), Requirement("class ReqMagicLevel", "", 110.0, 3) }),
            Item(Elixir, { "Elixir" }) });
    }

    CharacterItem Held(uint64 offset, uint32 templateId, uint32 slot)
    {
        CharacterItem item;
        item.Guid = ObjectGuid::ItemBase + offset;
        item.TemplateId = templateId;
        item.Slot = slot;
        return item;
    }

    class PlayerEquipmentTest : public testing::Test
    {
    protected:
        EquipmentSlots const _slots = Slots();
        std::shared_ptr<ItemTemplateStore const> const _templates = Templates();
        EquipWizard const _iceWizard{ "Ice", 10 };
    };
}

TEST_F(PlayerEquipmentTest, AFireOnlyRobeOnAnIceWizardIsRefusedAndStaysInTheBackpack)
{
    CharacterItem const robe = Held(1, FireRobe, 0);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ robe });
    PlayerEquipment equipment;

    EquipChange const change = equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, robe.Guid, "Robe");

    EXPECT_EQ(change.Result, EquipResult::RequirementsNotMet);
    EXPECT_FALSE(change.Worn);
    EXPECT_FALSE(change.Returned);
    EXPECT_EQ(equipment.Size(), 0u);
    ASSERT_NE(backpack.Find(robe.Guid), nullptr) << "a refused robe stays in the backpack";
    EXPECT_EQ(*backpack.Find(robe.Guid), robe);

    EquipWizard const fireWizard{ "fire", 1 };
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, fireWizard, robe.Guid, "Robe").Result, EquipResult::Equipped)
        << "a Fire wizard meets the same requirement, whatever the case of its school's name";
}

TEST_F(PlayerEquipmentTest, ARobeAboveTheWizardsLevelIsRefused)
{
    CharacterItem const robe = Held(2, HighLevelRobe, 0);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ robe });
    PlayerEquipment equipment;

    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, robe.Guid, "Robe").Result, EquipResult::RequirementsNotMet);
    EXPECT_NE(backpack.Find(robe.Guid), nullptr);
    EquipWizard const veteran{ "Ice", 110 };
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, veteran, robe.Guid, "Robe").Result, EquipResult::Equipped) << "level 110 meets ReqMagicLevel >= 110";
}

TEST_F(PlayerEquipmentTest, EquippingIntoAnOccupiedSlotMovesTheOldItemBackToTheBackpack)
{
    CharacterItem const worn = Held(3, StarterRobe, 0);
    CharacterItem const robe = Held(4, IceRobe, 5);
    CharacterItem const hat = Held(5, Hat, 6);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ robe, hat });
    PlayerEquipment equipment = PlayerEquipment::FromStored({ { worn, "Robe" } });

    EquipChange const change = equipment.Equip(backpack, 2, _slots, *_templates, _iceWizard, robe.Guid, "robe");

    ASSERT_EQ(change.Result, EquipResult::Equipped) << PlayerEquipment::ResultName(change.Result);
    ASSERT_TRUE(change.Worn);
    EXPECT_EQ(change.Worn->Item.Guid, robe.Guid);
    EXPECT_EQ(change.Worn->Slot, "Robe") << "the slot is stored under the name the equipment template gives it";
    ASSERT_TRUE(change.Returned);
    EXPECT_EQ(change.Returned->Guid, worn.Guid);
    EXPECT_EQ(change.Returned->Slot, 7u) << "the old robe arrives in the backpack after everything already there";
    EXPECT_EQ(change.ReturnedIndex, 0u);
    EXPECT_EQ(equipment.Size(), 1u);
    EXPECT_NE(equipment.Find(robe.Guid), nullptr);
    EXPECT_EQ(equipment.Find(worn.Guid), nullptr);
    EXPECT_EQ(backpack.Size(), 2u);
    EXPECT_NE(backpack.Find(worn.Guid), nullptr) << "the robe taken off is back in the backpack";
    EXPECT_EQ(backpack.Find(robe.Guid), nullptr);
}

TEST_F(PlayerEquipmentTest, ASwapIntoABackpackAlreadyOverItsCapacityIsRefused)
{
    CharacterItem const worn = Held(6, StarterRobe, 0);
    CharacterItem const robe = Held(7, IceRobe, 1);
    CharacterItem const hat = Held(8, Hat, 2);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ robe, hat });
    PlayerEquipment equipment = PlayerEquipment::FromStored({ { worn, "Robe" } });

    EXPECT_EQ(equipment.Equip(backpack, 1, _slots, *_templates, _iceWizard, robe.Guid, "Robe").Result, EquipResult::BackpackFull);
    EXPECT_NE(equipment.Find(worn.Guid), nullptr);
    EXPECT_NE(backpack.Find(robe.Guid), nullptr);
    EXPECT_EQ(backpack.Size(), 2u);
}

TEST_F(PlayerEquipmentTest, SlotsAreMatchedByTheirAdjectives)
{
    CharacterItem const wand = Held(9, Wand, 0);
    CharacterItem const hat = Held(10, Hat, 1);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ wand, hat });
    PlayerEquipment equipment;

    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, hat.Guid, "Robe").Result, EquipResult::WrongSlot);
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, hat.Guid, "Cape").Result, EquipResult::NoSuchSlot);
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, ObjectGuid::ItemBase + 99, "Hat").Result, EquipResult::NotOwned);
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, wand.Guid, "Weapon").Result, EquipResult::Equipped) << "a wand is one of the weapon slot's OR adjectives";
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, hat.Guid, "Hat").Result, EquipResult::Equipped);
    EXPECT_EQ(equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, hat.Guid, "Hat").Result, EquipResult::NotOwned) << "a worn item is no longer in the backpack";
}

TEST_F(PlayerEquipmentTest, ASlotHoldingSeveralItemsGivesBackItsOldestOnlyWhenFull)
{
    std::vector<CharacterItem> elixirs{ Held(11, Elixir, 0), Held(12, Elixir, 1), Held(13, Elixir, 2) };
    PlayerBackpack backpack = PlayerBackpack::FromStored(elixirs);
    PlayerEquipment equipment;

    EquipChange const first = equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, elixirs[0].Guid, "Elixir");
    EquipChange const second = equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, elixirs[1].Guid, "Elixir");
    EXPECT_FALSE(first.Returned);
    EXPECT_FALSE(second.Returned);
    EXPECT_EQ(equipment.CountIn("Elixir"), 2u);

    EquipChange const third = equipment.Equip(backpack, Roomy, _slots, *_templates, _iceWizard, elixirs[2].Guid, "Elixir");
    ASSERT_EQ(third.Result, EquipResult::Equipped);
    ASSERT_TRUE(third.Returned);
    EXPECT_EQ(third.Returned->Guid, elixirs[0].Guid);
    EXPECT_EQ(equipment.CountIn("Elixir"), 2u);
}

TEST_F(PlayerEquipmentTest, UnequippingReturnsTheItemToTheBackpackUnlessItIsFull)
{
    CharacterItem const worn = Held(14, Hat, 0);
    CharacterItem const robe = Held(15, IceRobe, 3);
    PlayerBackpack backpack = PlayerBackpack::FromStored({ robe });
    PlayerEquipment equipment = PlayerEquipment::FromStored({ { worn, "Hat" } });

    EXPECT_EQ(equipment.Unequip(backpack, 1, worn.Guid).Result, UnequipResult::BackpackFull);
    EXPECT_NE(equipment.Find(worn.Guid), nullptr);
    EXPECT_EQ(equipment.Unequip(backpack, Roomy, robe.Guid).Result, UnequipResult::NotWorn);

    UnequipChange const change = equipment.Unequip(backpack, 2, worn.Guid);
    ASSERT_EQ(change.Result, UnequipResult::Unequipped);
    ASSERT_TRUE(change.Returned);
    ASSERT_TRUE(change.TakenOff);
    EXPECT_EQ(change.TakenOff->Slot, "Hat");
    EXPECT_EQ(change.Index, 0u);
    EXPECT_EQ(change.Returned->Slot, 4u);
    EXPECT_EQ(equipment.Size(), 0u);
    EXPECT_NE(backpack.Find(worn.Guid), nullptr);
}
