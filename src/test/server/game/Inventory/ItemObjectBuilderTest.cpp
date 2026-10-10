/*
 * Project Ambrose by Imjustchico
 * Tests where a wizard's stored items go when it enters, that the public entry other wizards are sent of a worn item travels in CoreObject form behind a zero core header as the client reads it and is the WizardEquippedItemInfo the client keeps, with the item's own colors and pattern and no lock bit, the player object lists the equipment behavior before the backpack and both carry an m_itemList, so the items and the backpack's size land in the behavior that also carries m_numItemsAllowed, and a player with no such behavior is refused with the reason.
 */

#include "CharacterTypeFixtures.h"
#include "CoreObjectSerializer.h"
#include "ItemObjectBuilder.h"
#include "ObjectFields.h"
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
        AddClass(classes, "class EquippedItemInfo", Json::array({ "PropertyClass" }), {
            { "m_itemID", Property("unsigned int", "m_itemID", 0, Wire) } });
        AddClass(classes, "class WizardEquippedItemInfo", Json::array({ "EquippedItemInfo", "PropertyClass" }), {
            { "m_itemID", Property("unsigned int", "m_itemID", 0, Wire) },
            { "m_baseColor", Property("bui5", "m_baseColor", 1, Wire) },
            { "m_trimColor", Property("bui5", "m_trimColor", 2, Wire) },
            { "m_pattern", Property("bui5", "m_pattern", 3, Wire) },
            { "m_displayID", Property("unsigned int", "m_displayID", 4, Wire) },
            { "m_fullColor", Property("u24", "m_fullColor", 5, Wire) } });
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

TEST(ItemObjectBuilderTest, AWornItemsPublicEntryTravelsInCoreObjectFormBehindAZeroHeader)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    std::vector<std::string> errors;
    CoreObjectTypeTablePtr const types = CoreObjectTypeTable::Build({}, {}, *catalog, errors);
    ASSERT_TRUE(types) << (errors.empty() ? std::string() : errors.front());
    std::string problem;
    CharacterItem hat;
    hat.TemplateId = 1446127;
    PropertyObjectPtr const info = ItemObjectBuilder::BuildPublicInfo(catalog, hat, problem);
    ASSERT_TRUE(info) << problem;

    std::optional<std::string> const encoded = ItemObjectBuilder::EncodePublicInfo("MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM", *info, *types, problem);
    ASSERT_TRUE(encoded) << problem;
    ObjectField const* const field = ObjectFields::Find("MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM", "SerializedInfo");
    ASSERT_NE(field, nullptr);
    EXPECT_TRUE(field->CoreObjects);
    ObjectField plainField = *field;
    plainField.CoreObjects = false;
    EncodeResult const plain = ObjectSerializer::EncodeField(plainField, info.get());
    ASSERT_TRUE(plain.Ok()) << plain.Detail;
    ASSERT_EQ(encoded->size(), plain.Bytes.size() + 2) << "the client's core object reader takes a core type and a template type before the class hash";
    EXPECT_EQ(static_cast<uint8>((*encoded)[0]), 0u);
    EXPECT_EQ(static_cast<uint8>((*encoded)[1]), 0u);

    std::span<uint8 const> const bytes(reinterpret_cast<uint8 const*>(encoded->data()), encoded->size());
    DecodeResult const decoded = CoreObjectSerializer::DecodeField(catalog, *field, bytes, *types);
    ASSERT_TRUE(decoded.Object) << decoded.Detail;
    EXPECT_EQ(decoded.Object->GetClass().Name, "class WizardEquippedItemInfo");
    EXPECT_EQ(*decoded.Object->Get("m_itemID")->GetIf<uint32>(), 1446127u);
}

TEST(ItemObjectBuilderTest, AWornItemsPublicEntryCarriesItsOwnColorsAndPatternWithoutTheLock)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    std::vector<std::string> errors;
    CoreObjectTypeTablePtr const types = CoreObjectTypeTable::Build({}, {}, *catalog, errors);
    ASSERT_TRUE(types);
    CharacterItem hat;
    hat.TemplateId = 1446127;
    hat.PrimaryColor = 7;
    hat.SecondaryColor = 3;
    hat.Pattern = 2;
    hat.Locked = true;
    std::string problem;
    PropertyObjectPtr const info = ItemObjectBuilder::BuildPublicInfo(catalog, hat, problem);
    ASSERT_TRUE(info) << problem;
    std::optional<std::string> const encoded = ItemObjectBuilder::EncodePublicInfo("MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM", *info, *types, problem);
    ASSERT_TRUE(encoded) << problem;

    std::span<uint8 const> const bytes(reinterpret_cast<uint8 const*>(encoded->data()), encoded->size());
    DecodeResult const decoded = CoreObjectSerializer::DecodeField(catalog, *ObjectFields::Find("MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM", "SerializedInfo"), bytes, *types);
    ASSERT_TRUE(decoded.Object) << decoded.Detail;
    EXPECT_EQ(decoded.Object->GetClass().Name, "class WizardEquippedItemInfo") << "the client drops a public entry that is not a WizardEquippedItemInfo, so the wearer's hat is never tinted";
    EXPECT_EQ(*decoded.Object->Get("m_baseColor")->GetIf<uint32>(), 7u);
    EXPECT_EQ(*decoded.Object->Get("m_trimColor")->GetIf<uint32>(), 3u);
    EXPECT_EQ(*decoded.Object->Get("m_pattern")->GetIf<uint32>(), 2u) << "the lock bit is the wearer's own and never reaches the wizards around it";
}
