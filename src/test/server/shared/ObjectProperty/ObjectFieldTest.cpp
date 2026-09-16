/*
 * Project Ambrose by Imjustchico
 * Tests the ObjectProperty message field rules on classes the test invents under the field table's class names: enveloped fields round-tripping, an unwrapped or oversized envelope refused, a root of a class the field does not allow or a missing root refused on both sides, a class the type dump lacks reported, and the decode limits loaded from configuration with out-of-range values clamped and applied to every decode whose options carry none.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "ObjectSerializer.h"
#include "ScopeExit.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Wire }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", Json::array({ "PropertyClass" }) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() }, { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
        Json badge = Json::object();
        badge["m_badgeTitle"] = Property("std::string", "m_badgeTitle", 0);
        AddClass(classes, "class BadgeInfo", badge);
        Json badges = Json::object();
        badges["m_badges"] = Property("class SharedPointer<class BadgeInfo>", "m_badges", 0, "List");
        AddClass(classes, "class BadgeInfoList", badges);
        Json creation = Json::object();
        creation["m_name"] = Property("std::wstring", "m_name", 0);
        AddClass(classes, "class WizardCharacterCreationInfo", creation);

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "fields.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    PropertyObjectPtr MakeBadges(TypeCatalogPtr const& catalog)
    {
        PropertyObjectPtr list = PropertyObject::Create(catalog, "class BadgeInfoList");
        if (!list)
            return list;
        PropertyValue::List badges;
        for (char const* title : { "Badges_1", "Badges_2", "Badges_3" })
        {
            PropertyObjectPtr badge = PropertyObject::Create(catalog, "class BadgeInfo");
            EXPECT_EQ(badge->Set("m_badgeTitle", title), PropertySetResult::Ok);
            badges.emplace_back(std::move(badge));
        }
        EXPECT_EQ(list->Set("m_badges", std::move(badges)), PropertySetResult::Ok);
        return list;
    }

    SerializerLimits LoadLimits(std::string const& content, std::vector<std::string>& problems)
    {
        LogTestDirectory directory;
        ConfigMgr config;
        EXPECT_TRUE(config.LoadInitial(directory.Write("objects.conf", content)).Succeeded());
        problems.clear();
        return SerializerLimits::Load(config, &problems);
    }
}

TEST(ObjectFieldTest, TheTableNamesEachFieldOnce)
{
    std::set<std::pair<std::string_view, std::string_view>> seen;
    for (ObjectField const& field : ObjectFields::GetAll())
    {
        EXPECT_TRUE(seen.emplace(field.Message, field.Field).second) << field.Message << "." << field.Field;
        EXPECT_FALSE(field.Classes.empty()) << field.Message << "." << field.Field;
    }
    ObjectField const* const creation = ObjectFields::Find("MSG_CREATECHARACTER", "CreationInfo");
    ASSERT_NE(creation, nullptr);
    EXPECT_FALSE(creation->Enveloped);
    EXPECT_FALSE(creation->AllowNull);
    ObjectField const* const badges = ObjectFields::Find("MSG_BADGES", "BadgeInfo");
    ASSERT_NE(badges, nullptr);
    EXPECT_TRUE(badges->Enveloped);
    EXPECT_EQ(ObjectFields::Find("MSG_CREATECHARACTER", "Unknown"), nullptr);
}

TEST(ObjectFieldTest, EnvelopedFieldsRoundTripAndRefuseBadEnvelopes)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const badges = MakeBadges(catalog);
    ASSERT_TRUE(badges);
    ObjectField const& field = *ObjectFields::Find("MSG_BADGES", "BadgeInfo");

    EncodeResult const wrapped = ObjectSerializer::EncodeField(field, badges.get());
    ASSERT_TRUE(wrapped.Ok()) << wrapped.Detail;
    EncodeResult const plain = ObjectSerializer::EncodeCompact(badges.get());
    ASSERT_TRUE(plain.Ok()) << plain.Detail;
    EXPECT_NE(wrapped.Bytes, plain.Bytes);
    DecodeResult const decoded = ObjectSerializer::DecodeField(catalog, field, wrapped.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(*decoded.Object == *badges);

    DecodeResult const unwrapped = ObjectSerializer::DecodeField(catalog, field, plain.Bytes);
    EXPECT_EQ(unwrapped.Status, SerializerStatus::BadEnvelope);
    EXPECT_EQ(unwrapped.Detail.rfind("MSG_BADGES.BadgeInfo holds an envelope that cannot be opened: ", 0), 0u) << unwrapped.Detail;

    SerializerOptions tight;
    tight.Limits.emplace().MaxInflatedSize = plain.Bytes.size() - 1;
    DecodeResult const oversized = ObjectSerializer::DecodeField(catalog, field, wrapped.Bytes, tight);
    EXPECT_EQ(oversized.Status, SerializerStatus::BadEnvelope);
    EXPECT_FALSE(oversized.Object);
}

TEST(ObjectFieldTest, FieldsHoldTheirRootToAllowedClassesAndRequireOne)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const badges = MakeBadges(catalog);
    PropertyObjectPtr const creation = PropertyObject::Create(catalog, "class WizardCharacterCreationInfo");
    ASSERT_TRUE(badges && creation);
    ASSERT_EQ(creation->Set("m_name", u"Merle"), PropertySetResult::Ok);
    ObjectField const& field = *ObjectFields::Find("MSG_CREATECHARACTER", "CreationInfo");

    EncodeResult const encoded = ObjectSerializer::EncodeField(field, creation.get());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    EXPECT_EQ(encoded.Bytes, ObjectSerializer::EncodeCompact(creation.get()).Bytes);
    DecodeResult const decoded = ObjectSerializer::DecodeField(catalog, field, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_TRUE(*decoded.Object == *creation);

    DecodeResult const wrong = ObjectSerializer::DecodeField(catalog, field, ObjectSerializer::EncodeCompact(badges.get()).Bytes);
    EXPECT_EQ(wrong.Status, SerializerStatus::WrongClass);
    EXPECT_EQ(wrong.Detail, "MSG_CREATECHARACTER.CreationInfo: the object names class BadgeInfoList, which is not a class allowed here");
    DecodeResult const missing = ObjectSerializer::DecodeField(catalog, field, std::vector<uint8>{ 0, 0, 0, 0 });
    EXPECT_EQ(missing.Status, SerializerStatus::NullNotAllowed);

    EncodeResult const refused = ObjectSerializer::EncodeField(field, badges.get());
    EXPECT_EQ(refused.Status, SerializerStatus::WrongClass);
    EXPECT_EQ(refused.Detail, "MSG_CREATECHARACTER.CreationInfo cannot carry a class BadgeInfoList");
    EXPECT_TRUE(refused.Bytes.empty());
    EXPECT_EQ(ObjectSerializer::EncodeField(field, nullptr).Status, SerializerStatus::NullNotAllowed);

    constexpr std::array<std::string_view, 1> ghostClasses{ "class GhostInfo" };
    ObjectField const ghost{ "MSG_GHOST", "Data", ghostClasses, false, true };
    DecodeResult const unknown = ObjectSerializer::DecodeField(catalog, ghost, encoded.Bytes);
    EXPECT_EQ(unknown.Status, SerializerStatus::UnknownClass);
    EXPECT_EQ(unknown.Detail, "MSG_GHOST.Data allows class GhostInfo, which the type dump does not list as a property class");
}

TEST(ObjectFieldTest, LimitsLoadFromConfigurationClampedAndApplyToNewOptions)
{
    std::vector<std::string> problems;
    EXPECT_EQ(LoadLimits("", problems), SerializerLimits{});
    EXPECT_TRUE(problems.empty());

    SerializerLimits const loaded = LoadLimits("ObjectProperty.MaxDepth = 500\nObjectProperty.MaxObjects = 0\nObjectProperty.MaxContainerCount = 12\n"
        "ObjectProperty.MaxDecodedBytes = 100\nObjectProperty.MaxInflatedSize = 99999999999\n", problems);
    EXPECT_EQ(loaded.MaxDepth, 128u);
    EXPECT_EQ(loaded.MaxObjects, 1u);
    EXPECT_EQ(loaded.MaxContainerCount, 12u);
    EXPECT_EQ(loaded.MaxDecodedBytes, 65536u);
    EXPECT_EQ(loaded.MaxInflatedSize, std::size_t{ 256 } << 20);
    EXPECT_EQ(problems.size(), 4u);
    EXPECT_EQ(problems.front(), "ObjectProperty.MaxDepth = 500 is outside 1-128; using 128");

    SerializerLimits const previous = SerializerLimits::Current();
    ScopeExit const restore([&previous] { SerializerLimits::Apply(previous); });
    SerializerLimits::Apply(loaded);
    EXPECT_EQ(SerializerLimits::Current(), loaded);
    EXPECT_FALSE(SerializerOptions{}.Limits);
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const badges = MakeBadges(catalog);
    ASSERT_TRUE(badges);
    DecodeResult const limited = ObjectSerializer::DecodeCompact(catalog, ObjectSerializer::EncodeCompact(badges.get()).Bytes);
    EXPECT_EQ(limited.Status, SerializerStatus::TooManyObjects);
}
