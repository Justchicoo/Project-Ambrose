/*
 * Project Ambrose by Imjustchico
 * Tests the CoreObject form on classes the test invents: a ClientObject opens with block 2 type 2 and a WizClientObject with block 104 type 2, each followed by its template id and read back equal, while a nested behavior keeps the plain form, a nested item its own pair and a null its six zero bytes; the public mask leaves out what only the authority may see; an object whose header disagrees with the table, or that carries none where its class needs one, is refused; a pair nobody listed is named when read; the table refuses plain, repeated, unknown and non-CoreObject rows; and MSG_LOGINCOMPLETE's Data travels enveloped and cannot be read or written without the table.
 */

#include "BlobEnvelope.h"
#include "CoreObjectSerializer.h"
#include "ObjectFields.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 WirePublic = Wire | 4;
    constexpr uint32 Kept = 1 | 2 | 4 | 32;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = WirePublic, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    Json ObjectProperties()
    {
        Json properties = Json::object();
        properties["m_inactiveBehaviors"] = Property("class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors", 0, WirePublic, "List");
        properties["m_globalID.m_full"] = Property("unsigned __int64", "m_globalID.m_full", 1);
        properties["m_templateID.m_full"] = Property("unsigned __int64", "m_templateID.m_full", 2);
        properties["m_fScale"] = Property("float", "m_fScale", 3);
        return properties;
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class WizClientObjectItem", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), ObjectProperties());

        Json player = ObjectProperties();
        player["m_characterId"] = Property("unsigned __int64", "m_characterId", 4);
        player["m_secret"] = Property("unsigned int", "m_secret", 5, Wire);
        player["m_items"] = Property("class SharedPointer<class ClientObject>", "m_items", 6, WirePublic, "List");
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), player);

        Json instance = Json::object();
        instance["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0, Kept);
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        Json mobile = instance;
        mobile["m_speed"] = Property("float", "m_speed", 1);
        AddClass(classes, "class TestMobileBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), mobile);

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "core.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    std::vector<uint8> Header(uint8 block, uint8 type, uint32 id)
    {
        return { block, type, static_cast<uint8>(id), static_cast<uint8>(id >> 8), static_cast<uint8>(id >> 16), static_cast<uint8>(id >> 24) };
    }

    bool Holds(std::vector<uint8> const& bytes, std::vector<uint8> const& run)
    {
        return std::search(bytes.begin(), bytes.end(), run.begin(), run.end()) != bytes.end();
    }

    class CoreObjectSerializerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
            std::vector<std::string> errors;
            _types = CoreObjectTypeTable::Build({ { 2, 2, "class ClientObject" }, { 104, 2, "class WizClientObject" }, { 115, 9, "class WizClientObjectItem" } }, *_catalog, errors);
            ASSERT_TRUE(_types) << (errors.empty() ? std::string() : errors.front());
        }

        PropertyObjectPtr MakePlayer()
        {
            PropertyObjectPtr player = PropertyObject::Create(_catalog, "class WizClientObject");
            if (!player)
            {
                ADD_FAILURE() << "the player did not build";
                return player;
            }
            player->SetCoreHeader(CoreObjectHeader{ 104, 2, 1 });
            EXPECT_EQ(player->Set("m_globalID.m_full", uint64{ 0x0123456789ABCDEFull }), PropertySetResult::Ok);
            EXPECT_EQ(player->Set("m_templateID.m_full", uint64{ 1 }), PropertySetResult::Ok);
            EXPECT_EQ(player->Set("m_fScale", 1.0f), PropertySetResult::Ok);
            EXPECT_EQ(player->Set("m_characterId", uint64{ 1 }), PropertySetResult::Ok);
            EXPECT_EQ(player->Set("m_secret", uint32{ 0xC0FFEE }), PropertySetResult::Ok);

            PropertyObjectPtr mobile = PropertyObject::Create(_catalog, "class TestMobileBehavior");
            EXPECT_TRUE(mobile);
            if (mobile)
            {
                EXPECT_EQ(mobile->Set("m_speed", 3.5f), PropertySetResult::Ok);
            }
            PropertyValue::List behaviors;
            behaviors.emplace_back(std::move(mobile));
            behaviors.emplace_back(PropertyObjectPtr());
            EXPECT_EQ(player->Set("m_inactiveBehaviors", std::move(behaviors)), PropertySetResult::Ok);

            PropertyObjectPtr hat = PropertyObject::Create(_catalog, "class WizClientObjectItem");
            EXPECT_TRUE(hat);
            if (hat)
            {
                hat->SetCoreHeader(CoreObjectHeader{ 115, 9, 9001 });
                EXPECT_EQ(hat->Set("m_templateID.m_full", uint64{ 9001 }), PropertySetResult::Ok);
            }
            PropertyValue::List items;
            items.emplace_back(std::move(hat));
            EXPECT_EQ(player->Set("m_items", std::move(items)), PropertySetResult::Ok);
            return player;
        }

        TypeCatalogPtr _catalog;
        CoreObjectTypeTablePtr _types;
    };
}

TEST_F(CoreObjectSerializerTest, AClientObjectOpensWithTwoTwoAndItsTemplateAndReadsBackEqual)
{
    PropertyObjectPtr const object = PropertyObject::Create(_catalog, "class ClientObject");
    ASSERT_TRUE(object);
    object->SetCoreHeader(CoreObjectHeader{ 2, 2, 7 });
    ASSERT_EQ(object->Set("m_globalID.m_full", uint64{ 42 }), PropertySetResult::Ok);

    EncodeResult const encoded = CoreObjectSerializer::Encode(*object, *_types);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    ASSERT_GE(encoded.Bytes.size(), 6u);
    EXPECT_EQ(std::vector<uint8>(encoded.Bytes.begin(), encoded.Bytes.begin() + 6), Header(2, 2, 7));

    DecodeResult const decoded = CoreObjectSerializer::Decode(_catalog, encoded.Bytes, *_types);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.BytesRead, encoded.Bytes.size());
    EXPECT_EQ(*decoded.Object, *object);
    ASSERT_TRUE(decoded.Header);
    EXPECT_EQ(*decoded.Header, (CoreObjectHeader{ 2, 2, 7 }));
}

TEST_F(CoreObjectSerializerTest, AWizClientObjectOpensWith104TwoAndWhatItHoldsKeepsItsOwnForm)
{
    PropertyObjectPtr const player = MakePlayer();
    ASSERT_TRUE(player);
    EncodeResult const encoded = CoreObjectSerializer::Encode(*player, *_types);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    ASSERT_GE(encoded.Bytes.size(), 6u);
    EXPECT_EQ(std::vector<uint8>(encoded.Bytes.begin(), encoded.Bytes.begin() + 6), Header(104, 2, 1)) << "68 02 01000000, the header every accepted player object opens with";
    EXPECT_TRUE(Holds(encoded.Bytes, Header(0, 0, StringHash::KiStringHash("class TestMobileBehavior")))) << "a behavior is written plain";
    EXPECT_TRUE(Holds(encoded.Bytes, Header(115, 9, 9001))) << "an item is written with its own pair and template";
    EXPECT_TRUE(Holds(encoded.Bytes, Header(0, 0, 0))) << "a null is six zero bytes";

    DecodeResult const decoded = CoreObjectSerializer::Decode(_catalog, encoded.Bytes, *_types);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.BytesRead, encoded.Bytes.size());
    EXPECT_EQ(*decoded.Object, *player);

    PropertyValue::List const& behaviors = *decoded.Object->Get("m_inactiveBehaviors")->GetList();
    ASSERT_EQ(behaviors.size(), 2u);
    ASSERT_NE(behaviors[0].AsObject(), nullptr);
    EXPECT_FALSE(behaviors[0].AsObject()->GetCoreHeader().has_value());
    EXPECT_TRUE(behaviors[1].IsNullObject());
    PropertyValue::List const& items = *decoded.Object->Get("m_items")->GetList();
    ASSERT_EQ(items.size(), 1u);
    ASSERT_NE(items[0].AsObject(), nullptr);
    ASSERT_TRUE(items[0].AsObject()->GetCoreHeader().has_value());
    EXPECT_EQ(*items[0].AsObject()->GetCoreHeader(), (CoreObjectHeader{ 115, 9, 9001 }));
}

TEST_F(CoreObjectSerializerTest, ThePublicMaskLeavesOutWhatOnlyTheAuthoritySees)
{
    PropertyObjectPtr const player = MakePlayer();
    ASSERT_TRUE(player);
    SerializerOptions owner;
    owner.Mask = SerializerOptions::TransmitMask;
    SerializerOptions everyone;
    everyone.Mask = SerializerOptions::PublicMask;
    EncodeResult const own = CoreObjectSerializer::Encode(*player, *_types, owner);
    EncodeResult const shown = CoreObjectSerializer::Encode(*player, *_types, everyone);
    ASSERT_TRUE(own.Ok()) << own.Detail;
    ASSERT_TRUE(shown.Ok()) << shown.Detail;
    EXPECT_EQ(own.Bytes.size(), shown.Bytes.size() + sizeof(uint32)) << "m_secret is the one property without Public";

    DecodeResult const ownDecoded = CoreObjectSerializer::Decode(_catalog, own.Bytes, *_types, owner);
    DecodeResult const shownDecoded = CoreObjectSerializer::Decode(_catalog, shown.Bytes, *_types, everyone);
    ASSERT_TRUE(ownDecoded.Ok()) << ownDecoded.Detail;
    ASSERT_TRUE(shownDecoded.Ok()) << shownDecoded.Detail;
    EXPECT_EQ(*ownDecoded.Object->Get("m_secret")->GetIf<uint32>(), 0xC0FFEEu);
    EXPECT_EQ(*shownDecoded.Object->Get("m_secret")->GetIf<uint32>(), 0u) << "left out, so it reads back as its default";
    EXPECT_EQ(*shownDecoded.Object->Get("m_characterId")->GetIf<uint64>(), 1u);
}

TEST_F(CoreObjectSerializerTest, AnObjectMustCarryThePairTheTableGivesItsClass)
{
    PropertyObjectPtr const bare = PropertyObject::Create(_catalog, "class ClientObject");
    ASSERT_TRUE(bare);
    EncodeResult const missing = CoreObjectSerializer::Encode(*bare, *_types);
    EXPECT_EQ(missing.Status, SerializerStatus::WrongClass);
    EXPECT_NE(missing.Detail.find("carries no template id"), std::string::npos) << missing.Detail;

    bare->SetCoreHeader(CoreObjectHeader{ 104, 2, 1 });
    EncodeResult const wrong = CoreObjectSerializer::Encode(*bare, *_types);
    EXPECT_EQ(wrong.Status, SerializerStatus::WrongClass);
    EXPECT_NE(wrong.Detail.find("carries core object block 104 and type 2, where the core object table gives block 2 and type 2"), std::string::npos) << wrong.Detail;

    PropertyObjectPtr const behavior = PropertyObject::Create(_catalog, "class TestMobileBehavior");
    ASSERT_TRUE(behavior);
    behavior->SetCoreHeader(CoreObjectHeader{ 2, 2, 5 });
    EncodeResult const unlisted = CoreObjectSerializer::Encode(*behavior, *_types);
    EXPECT_EQ(unlisted.Status, SerializerStatus::WrongClass);
    EXPECT_NE(unlisted.Detail.find("where the core object table gives it none"), std::string::npos) << unlisted.Detail;

    EncodeResult const plain = ObjectSerializer::Encode(bare.get());
    EXPECT_TRUE(plain.Ok()) << "without a table the object is written in the plain form and its header is not consulted";
}

TEST_F(CoreObjectSerializerTest, APairTheTableDoesNotListIsNamedWhenRead)
{
    std::vector<uint8> bytes = Header(9, 9, 1);
    DecodeResult const decoded = CoreObjectSerializer::Decode(_catalog, bytes, *_types);
    EXPECT_EQ(decoded.Status, SerializerStatus::UnknownClass);
    ASSERT_TRUE(decoded.UnknownCore);
    EXPECT_EQ(decoded.UnknownCore->Block, 9);
    EXPECT_EQ(decoded.UnknownCore->Type, 9);
    ASSERT_TRUE(decoded.Header) << "the root's header is reported even when the read fails";
    EXPECT_EQ(*decoded.Header, (CoreObjectHeader{ 9, 9, 1 }));
}

TEST_F(CoreObjectSerializerTest, TheTableRefusesRowsTheClientCouldNeverMeanAndNamesEachOne)
{
    std::vector<std::string> errors;
    CoreObjectTypeTablePtr const table = CoreObjectTypeTable::Build({
        { 0, 0, "class ClientObject" },
        { 2, 2, "class ClientObject" },
        { 2, 2, "class WizClientObject" },
        { 3, 3, "class ClientObject" },
        { 4, 4, "class Nowhere" },
        { 5, 5, "class TestMobileBehavior" } }, *_catalog, errors);
    EXPECT_FALSE(table);
    ASSERT_EQ(errors.size(), 5u);
    EXPECT_NE(errors[0].find("block 0 type 0 is the pair that says a plain class hash follows"), std::string::npos) << errors[0];
    EXPECT_NE(errors[1].find("block 2 type 2 is listed for both class ClientObject and class WizClientObject"), std::string::npos) << errors[1];
    EXPECT_NE(errors[2].find("class ClientObject is given both block 2 type 2 and block 3 type 3"), std::string::npos) << errors[2];
    EXPECT_NE(errors[3].find("names class Nowhere, which the type dump does not list"), std::string::npos) << errors[3];
    EXPECT_NE(errors[4].find("names class TestMobileBehavior, which is not a class CoreObject"), std::string::npos) << errors[4];

    ASSERT_NE(_types->Find(104, 2), nullptr);
    EXPECT_EQ(_types->Find(104, 2)->ClassName, "class WizClientObject");
    ASSERT_NE(_types->FindByClass(StringHash::KiStringHash("class WizClientObjectItem")), nullptr);
    EXPECT_EQ(_types->FindByClass(StringHash::KiStringHash("class WizClientObjectItem"))->Block, 115);
    EXPECT_EQ(_types->Find(9, 9), nullptr);
    EXPECT_EQ(_types->Count(), 3u);
}

TEST_F(CoreObjectSerializerTest, LoginCompleteDataTravelsEnvelopedAndNeedsTheTable)
{
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "Data");
    ASSERT_NE(field, nullptr);
    EXPECT_TRUE(field->Enveloped);
    EXPECT_TRUE(field->CoreObjects);
    EXPECT_FALSE(field->AllowNull);

    PropertyObjectPtr const player = MakePlayer();
    ASSERT_TRUE(player);
    EncodeResult const refused = ObjectSerializer::EncodeField(*field, player.get());
    EXPECT_EQ(refused.Status, SerializerStatus::UnknownClass);
    EXPECT_NE(refused.Detail.find("without the core object table"), std::string::npos) << refused.Detail;

    EncodeResult const encoded = CoreObjectSerializer::EncodeField(*field, *player, *_types);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    BlobEnvelope::UnwrapResult const opened = BlobEnvelope::Unwrap(encoded.Bytes, 1 << 20);
    ASSERT_TRUE(opened.Succeeded());
    EXPECT_EQ(opened.Packed, BlobEnvelope::Packing::Compress);
    ASSERT_GE(opened.Data.size(), 6u);
    EXPECT_EQ(std::vector<uint8>(opened.Data.begin(), opened.Data.begin() + 6), Header(104, 2, 1));

    DecodeResult const unreadable = ObjectSerializer::DecodeField(_catalog, *field, encoded.Bytes);
    EXPECT_EQ(unreadable.Status, SerializerStatus::UnknownClass);
    DecodeResult const decoded = CoreObjectSerializer::DecodeField(_catalog, *field, encoded.Bytes, *_types);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object, *player);
}
