/*
 * Project Ambrose by Imjustchico
 * Writes the item template test install through the writer's dump, which adds to the reader's the classes it leaves out, so the reader skips them as it would a class the client's dump lacks.
 */

#include "ItemTemplateFixtures.h"

#include <gtest/gtest.h>

#include <utility>

using namespace TemplateDumpFixtures;

namespace
{
    std::string ItemDump(bool withUnknown)
    {
        Json classes = Json::object();
        AddTemplateClasses(classes);
        AddSpellClasses(classes);
        AddItemClasses(classes, withUnknown);
        if (withUnknown)
        {
            Json shop = Json::object();
            shop["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
            shop["m_shopName"] = Property("std::string", "m_shopName", 1);
            AddClass(classes, ItemTemplateFixtures::UnknownBehavior, Json::array({ "BehaviorTemplate", "PropertyClass" }), shop);
        }
        return Dump(classes);
    }
}

ItemTemplateFixtures::ItemTemplateFixtures() : _writer(&_views)
{
    EXPECT_TRUE(_writer.LoadFromText(ItemDump(true), "writer.json")) << _writer.GetErrors().front();
}

std::string ItemTemplateFixtures::ReaderDump()
{
    return ItemDump(false);
}

PropertyObjectPtr ItemTemplateFixtures::Create(std::string const& type)
{
    PropertyObjectPtr object = PropertyObject::Create(_writer.GetCatalog(), type);
    EXPECT_TRUE(object) << type;
    return object;
}

PropertyObjectPtr ItemTemplateFixtures::Item(std::string const& name, std::string const& display, std::string const& school, float cost, int32 rank, int32 limit, uint32 setBonus)
{
    PropertyObjectPtr item = Create("class WizItemTemplate");
    EXPECT_EQ(item->Set("m_objectName", std::string(name)), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_displayName", std::string(display)), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_nObjectType", int64{ 3 }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_school", std::string(school)), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_baseCost", float{ cost }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_rank", int32{ rank }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_itemLimit", int32{ limit }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_itemSetBonusTemplateID", uint32{ setBonus }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_numPrimaryColors", uint8{ 2 }), PropertySetResult::Ok);
    EXPECT_EQ(item->Set("m_numSecondaryColors", uint8{ 1 }), PropertySetResult::Ok);
    PropertyValue::List adjectives;
    adjectives.emplace_back(std::string("Hat"));
    adjectives.emplace_back(std::string(school));
    EXPECT_EQ(item->Set("m_adjectiveList", std::move(adjectives)), PropertySetResult::Ok);
    return item;
}

void ItemTemplateFixtures::Write(std::filesystem::path const& gameData, float hatCost, Robe robe)
{
    PropertyObjectPtr hat = Item("WC-Hat-Fire-01", "Items_00028316", "Fire", hatCost, 1, 0, 0);
    PropertyObjectPtr robeItem = Item("WC-Robe-Ice-02", "Items_00028400", "Ice", 300.0f, 5, 1, 42);
    PropertyObjectPtr shop = Create(UnknownBehavior);
    EXPECT_EQ(shop->Set("m_behaviorName", std::string("ShopBehavior")), PropertySetResult::Ok);
    PropertyValue::List behaviors;
    behaviors.emplace_back(std::move(shop));
    EXPECT_EQ(robeItem->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
    PropertyValue::List effects;
    PropertyObjectPtr effect = Create(robe == Robe::UnknownEffect ? UnknownEffect : "class GameEffectInfo");
    EXPECT_EQ(effect->Set("m_effectName", std::string("MaxHealth")), PropertySetResult::Ok);
    effects.emplace_back(std::move(effect));
    EXPECT_EQ(robeItem->Set("m_equipEffects", std::move(effects)), PropertySetResult::Ok);
    PropertyObjectPtr requirement = Create(robe == Robe::UnknownRequirement ? UnknownRequirement : "class ReqMagicLevel");
    if (robe == Robe::UnknownRequirement)
    {
        EXPECT_EQ(requirement->Set("m_badgeName", std::string("Ice Elemental")), PropertySetResult::Ok);
    }
    else
    {
        EXPECT_EQ(requirement->Set("m_numericValue", float{ 5 }), PropertySetResult::Ok);
        EXPECT_EQ(requirement->Set("m_operatorType", int32{ 3 }), PropertySetResult::Ok);
        EXPECT_EQ(requirement->Set("m_magicSchool", std::string("Ice")), PropertySetResult::Ok);
    }
    PropertyValue::List requirements;
    requirements.emplace_back(std::move(requirement));
    PropertyObjectPtr list = Create("class RequirementList");
    EXPECT_EQ(list->Set("m_operator", int64{ 1 }), PropertySetResult::Ok);
    EXPECT_EQ(list->Set("m_requirements", std::move(requirements)), PropertySetResult::Ok);
    EXPECT_EQ(robeItem->Set("m_equipRequirements", std::move(list)), PropertySetResult::Ok);

    PropertyObjectPtr npc = Create("class GameObjectTemplate");
    EXPECT_EQ(npc->Set("m_objectName", std::string("WC-RAV-NPC06")), PropertySetResult::Ok);
    PropertyObjectPtr set = Create("class ItemSetBonusTemplate");
    EXPECT_EQ(set->Set("m_objectName", std::string("ItemSet-Ice-02")), PropertySetResult::Ok);
    EXPECT_EQ(set->Set("m_displayName", std::string("ItemSets_00000042")), PropertySetResult::Ok);
    EXPECT_EQ(set->Set("m_noStacking", true), PropertySetResult::Ok);
    PropertyValue::List bonuses;
    for (int32 const items : { 2, 3 })
    {
        PropertyObjectPtr bonus = Create("class ItemSetBonusData");
        EXPECT_EQ(bonus->Set("m_numItemsToEquip", int32{ items }), PropertySetResult::Ok);
        PropertyValue::List granted;
        for (int32 grantedCount = 0; grantedCount < items - 1; ++grantedCount)
        {
            PropertyObjectPtr accuracy = Create("class GameEffectInfo");
            EXPECT_EQ(accuracy->Set("m_effectName", std::string("CanonicalIceAccuracy")), PropertySetResult::Ok);
            granted.emplace_back(std::move(accuracy));
        }
        EXPECT_EQ(bonus->Set("m_equipEffectsGranted", std::move(granted)), PropertySetResult::Ok);
        bonuses.emplace_back(std::move(bonus));
    }
    EXPECT_EQ(set->Set("m_itemSetBonusDataList", std::move(bonuses)), PropertySetResult::Ok);
    PropertyObjectPtr spell = Create("class SpellTemplate");
    EXPECT_EQ(spell->Set("m_name", std::string("Not An Item")), PropertySetResult::Ok);

    Files const files{ { "ObjectData/Items/Hats/WC-Hat-Fire-01.xml", WriteBind(hat) }, { "ObjectData/Items/Robes/WC-Robe-Ice-02.xml", WriteBind(robeItem) },
        { "ObjectData/WC/WC-RAV-NPC06.xml", WriteBind(npc) }, { "ObjectData/ItemSets/ItemSet-Ice-02.xml", WriteBind(set) }, { "Spells/Not An Item.xml", WriteBind(spell) } };
    Locations const locations{ { HatId, "ObjectData/Items/Hats/WC-Hat-Fire-01.xml" }, { RobeId, "ObjectData/Items/Robes/WC-Robe-Ice-02.xml" },
        { NpcId, "ObjectData/WC/WC-RAV-NPC06.xml" }, { SetBonusId, "ObjectData/ItemSets/ItemSet-Ice-02.xml" }, { SpellId, "Spells/Not An Item.xml" } };
    TemplateDumpFixtures::WriteRoot(gameData, _writer.GetCatalog(), locations, files);
}
