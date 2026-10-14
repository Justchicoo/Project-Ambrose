/*
 * Project Ambrose by Imjustchico
 * Tests the messages that show a wizard its Dynamods: the full set reads back as a DynaModList opening with an empty placeholder at index 1, then each mod's tag, originator and index but not its zone name or zone id, which the transmit mask leaves out, a mod that names a state going as a DynaModDelta carrying the state's string id; a replaced mod drops the old by its index and adds the new, a removal or a mod for another zone tells the client nothing, a mod's state enters by its string id, and nothing is built without a catalog that holds the classes.
 */

#include "CharacterTypeFixtures.h"
#include "DynaModMessages.h"
#include "ObjectSerializer.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <span>
#include <string>

namespace
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using CharacterTypeFixtures::Detail::Property;

    constexpr std::string_view Hub = "WizardCity/WC_Hub";
    constexpr uint64 Player = 0x1122334455667788ull;

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "class DynaMod", Json::array({ "PropertyClass" }), {
            { "m_clientTag", Property("std::string", "m_clientTag", 0, 63) },
            { "m_zoneName", Property("std::string", "m_zoneName", 1, 39) },
            { "m_zoneInstance", Property("gid", "m_zoneInstance", 2, 39) },
            { "m_originator", Property("gid", "m_originator", 3, 63) },
            { "m_index", Property("int", "m_index", 4, 30) },
            { "m_zoneID", Property("unsigned int", "m_zoneID", 5, 16) } });
        AddClass(classes, "class DynaModDelta", Json::array({ "DynaMod", "PropertyClass" }), {
            { "m_clientTag", Property("std::string", "m_clientTag", 0, 63) },
            { "m_zoneName", Property("std::string", "m_zoneName", 1, 39) },
            { "m_zoneInstance", Property("gid", "m_zoneInstance", 2, 39) },
            { "m_originator", Property("gid", "m_originator", 3, 63) },
            { "m_index", Property("int", "m_index", 4, 30) },
            { "m_zoneID", Property("unsigned int", "m_zoneID", 5, 16) },
            { "m_remove", Property("bool", "m_remove", 6, 63) },
            { "m_state", Property("std::string", "m_state", 7, 39) },
            { "m_stateID", Property("unsigned int", "m_stateID", 8, 31) } });
        AddClass(classes, "class DynaModList", Json::array({ "PropertyClass" }), {
            { "m_allMods", Property("class SharedPointer<class DynaMod>", "m_allMods", 0, 63, "List") } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump(), "dynamod.json"))
            << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    DecodeResult Read(TypeCatalogPtr const& catalog, std::string const& bytes)
    {
        return ObjectSerializer::Decode(catalog, std::span(reinterpret_cast<uint8 const*>(bytes.data()), bytes.size()), SerializerOptions());
    }

    void ExpectMod(PropertyObject const& object, DynaMod const& mod)
    {
        EXPECT_EQ(*object.Get("m_clientTag")->GetIf<std::string>(), mod.Tag);
        EXPECT_EQ(*object.Get("m_originator")->GetIf<uint64>(), mod.Originator);
        EXPECT_EQ(*object.Get("m_index")->GetIf<int32>(), mod.Index);
        EXPECT_EQ(*object.Get("m_zoneID")->GetIf<uint32>(), 0u) << "the zone id carries only the authority bit, so the transmit mask leaves it out";
        EXPECT_TRUE(object.Get("m_zoneName")->GetIf<std::string>()->empty()) << "the zone name is never sent";
        if (mod.State.empty())
        {
            EXPECT_EQ(object.GetClass().Name, "class DynaMod");
            return;
        }
        EXPECT_EQ(object.GetClass().Name, "class DynaModDelta") << "a mod that names a state carries it so the client moves its object there itself";
        EXPECT_EQ(*object.Get("m_stateID")->GetIf<uint32>(), StringHash::StringId(mod.State));
        EXPECT_FALSE(*object.Get("m_remove")->GetIf<bool>());
        EXPECT_TRUE(object.Get("m_state")->GetIf<std::string>()->empty()) << "the state's name is never sent, only its string id";
    }
}

TEST(DynaModMessagesTest, TheFullSetReadsBackAsADynaModListOfEachMod)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    std::vector<DynaMod> const mods{ { std::string(Hub), "PetParkGate", "IdleOpen", false, 2, 9 }, { std::string(Hub), "Ultra_Rattlebones_40", "", false, 4, 9 } };
    std::string problem;
    std::optional<GameMessages::DynaModBehaviorUpdateMods> const message = DynaModMessages::All(catalog, Player, mods, problem);
    ASSERT_TRUE(message) << problem;
    EXPECT_EQ(message->GlobalId, Player);
    EXPECT_EQ(message->UpdateAll, 1);
    EXPECT_EQ(message->Add, 0);
    EXPECT_EQ(message->Remove, 0);

    DecodeResult const decoded = Read(catalog, message->AllMods);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.BytesRead, message->AllMods.size());
    PropertyValue::List const* const list = decoded.Object->Get("m_allMods")->GetList();
    ASSERT_TRUE(list);
    ASSERT_EQ(list->size(), 3u);
    PropertyObject const* const placeholder = (*list)[0].AsObject();
    ASSERT_TRUE(placeholder);
    ExpectMod(*placeholder, DynaMod{ "", "", "", false, WizardDynaMods::PlaceholderIndex, 0 });
    for (std::size_t at = 0; at < mods.size(); ++at)
    {
        PropertyObject const* const object = (*list)[at + 1].AsObject();
        ASSERT_TRUE(object);
        ExpectMod(*object, mods[at]);
    }

    std::optional<GameMessages::DynaModBehaviorUpdateMods> const none = DynaModMessages::All(catalog, Player, {}, problem);
    ASSERT_TRUE(none) << problem;
    DecodeResult const empty = Read(catalog, none->AllMods);
    ASSERT_TRUE(empty.Ok() && empty.Object) << empty.Detail;
    EXPECT_EQ(empty.Object->Get("m_allMods")->GetList()->size(), 1u) << "a wizard with no mods is still sent the placeholder";
}

TEST(DynaModMessagesTest, AReplacedModDropsTheOldByIndexAndAddsTheNewWhileOthersTellNothing)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    std::string problem;
    DynaModChange replaced;
    replaced.Removed = DynaMod{ std::string(Hub), "PetParkGate", "IdleClosed", false, 2, 9 };
    replaced.Added = DynaMod{ std::string(Hub), "PetParkGate", "IdleOpen", false, 3, 9 };
    std::optional<GameMessages::DynaModBehaviorUpdateMods> const message = DynaModMessages::Change(catalog, Player, Hub, replaced, problem);
    ASSERT_TRUE(message) << problem;
    EXPECT_EQ(message->UpdateAll, 0);
    EXPECT_EQ(message->Remove, 1);
    EXPECT_EQ(message->Index, 2);
    EXPECT_EQ(message->Add, 1);
    DecodeResult const decoded = Read(catalog, message->NewMod);
    ASSERT_TRUE(decoded.Ok() && decoded.Object) << decoded.Detail;
    ExpectMod(*decoded.Object, *replaced.Added);

    DynaModChange dropped;
    dropped.Removed = replaced.Added;
    std::optional<GameMessages::DynaModBehaviorUpdateMods> const drop = DynaModMessages::Change(catalog, Player, Hub, dropped, problem);
    ASSERT_TRUE(drop) << problem;
    EXPECT_EQ(drop->Remove, 1);
    EXPECT_EQ(drop->Index, 3);
    EXPECT_EQ(drop->Add, 0) << "a mod a quest result takes away is dropped by its index and nothing is added";

    DynaModChange removal;
    removal.Added = DynaMod{ std::string(Hub), "PetParkCollision", "", true, 4, 9 };
    EXPECT_FALSE(DynaModMessages::Change(catalog, Player, Hub, removal, problem));
    EXPECT_TRUE(problem.empty()) << "a removal arms no spawner, so the client is told nothing";

    EXPECT_FALSE(DynaModMessages::Change(catalog, Player, "WizardCity/WC_Ravenwood", replaced, problem)) << "a mod for another zone waits until the wizard is there";

    EXPECT_FALSE(DynaModMessages::All(nullptr, Player, {}, problem));
    EXPECT_FALSE(problem.empty());
}

TEST(DynaModMessagesTest, AModsStateEntersByItsStringIdAndAModWithNoneEntersNothing)
{
    std::optional<GameMessages::EnterState> const open = DynaModMessages::State(77, DynaMod{ std::string(Hub), "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 1, 9 });
    ASSERT_TRUE(open);
    EXPECT_EQ(open->GameObjectId, 77u);
    EXPECT_EQ(open->State, StringHash::StringId("IdleOpen"));
    EXPECT_FALSE(DynaModMessages::State(77, DynaMod{ std::string(Hub), "Ultra_Rattlebones_40", "", false, 2, 9 }));
    EXPECT_FALSE(DynaModMessages::State(77, DynaMod{ std::string(Hub), "PetParkCollision", "Open", true, 3, 9 }));
}
