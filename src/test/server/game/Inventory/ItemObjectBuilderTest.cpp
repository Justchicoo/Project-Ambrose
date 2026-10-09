/*
 * Project Ambrose by Imjustchico
 * Tests where a wizard's stored items go when it enters: the player object lists the equipment behavior before the backpack and both carry an m_itemList, so the items and the backpack's size land in the behavior that also carries m_numItemsAllowed, and a player with no such behavior is refused with the reason.
 */

#include "CharacterTypeFixtures.h"
#include "ItemObjectBuilder.h"
#include "PropertyFiller.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using CharacterTypeFixtures::Detail::Property;

    constexpr uint32 Wire = 0x1F;
    constexpr uint32 Local = 0x27;

    TypeCatalogPtr LoadCatalog()
    {
        Json dump = Json::parse(CharacterTypeFixtures::Dump());
        Json& classes = dump["classes"];
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), {});
        AddClass(classes, "class WizClientObject", Json::array({ "CoreObject", "PropertyClass" }), {
            { "m_inactiveBehaviors", Property("class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors", 0, Wire, "List") } });
        AddClass(classes, "class ClientWizEquipmentBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) },
            { "m_itemList", Property("class SharedPointer<class CoreObject>", "m_itemList", 1, Wire, "List") } });
        AddClass(classes, "class ClientWizInventoryBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) },
            { "m_itemList", Property("class SharedPointer<class CoreObject>", "m_itemList", 1, Wire, "List") },
            { "m_numItemsAllowed", Property("int", "m_numItemsAllowed", 2, Wire) } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(dump.dump(), "items.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    PropertyObjectPtr Player(TypeCatalogPtr const& catalog, std::vector<std::string> const& behaviors)
    {
        PropertyObjectPtr player = PropertyObject::Create(catalog, "class WizClientObject");
        if (!player)
            return nullptr;
        PropertyValue::List list;
        for (std::string const& name : behaviors)
            list.emplace_back(PropertyObject::Create(catalog, name));
        std::string problem;
        PropertyFiller(*player, problem).Set("m_inactiveBehaviors", std::move(list));
        if (!problem.empty())
            return nullptr;
        return player;
    }

    PropertyObject const* Behavior(PropertyObject const& player, std::size_t index)
    {
        return player.Get("m_inactiveBehaviors")->GetList()->at(index).AsObject();
    }
}

TEST(ItemObjectBuilderTest, TheBackpackIsTheBehaviorWithASizeNotTheEquipmentListedBeforeIt)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const player = Player(catalog, { "class ClientWizEquipmentBehavior", "class ClientWizInventoryBehavior" });
    ASSERT_TRUE(player);

    PropertyValue::List behaviors = *player->Get("m_inactiveBehaviors")->GetList();
    PropertyObject const* const found = ItemObjectBuilder::FindBackpack(behaviors);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetClass().Name, "class ClientWizInventoryBehavior") << "items given to the equipment list come back worn and cannot be trashed";

    std::string problem;
    ASSERT_TRUE(ItemObjectBuilder::SetItemsAllowed(*player, 100, problem)) << problem;
    EXPECT_EQ(*Behavior(*player, 1)->Get("m_numItemsAllowed")->GetIf<int32>(), 100);
}

TEST(ItemObjectBuilderTest, APlayerWithoutABackpackIsRefusedWithTheReason)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const player = Player(catalog, { "class ClientWizEquipmentBehavior" });
    ASSERT_TRUE(player);

    PropertyValue::List behaviors = *player->Get("m_inactiveBehaviors")->GetList();
    EXPECT_EQ(ItemObjectBuilder::FindBackpack(behaviors), nullptr) << "the equipment list is never taken for the backpack";
    std::string problem;
    EXPECT_FALSE(ItemObjectBuilder::SetItemsAllowed(*player, 100, problem));
    EXPECT_NE(problem.find("m_numItemsAllowed"), std::string::npos) << problem;
}
