/*
 * Project Ambrose by Imjustchico
 * Tests the type registry on small dumps written by the test with invented classes: aliases collapsed into their class, into an unprefixed template class, or standing in for a missing one; base chains; properties in id order found by hash and name; per-property enum options in both directions with text options, integer and text defaults in dump order and the base class hint; value, primitive and bit kinds; the class kind counts; and loads refused while the active catalog keeps serving: bad hashes, unknown bases and types, broken, empty or misshapen JSON, fields of the wrong JSON type or missing, duplicates, id gaps, oversized values, bad containers, keys that differ from the hash, inconsistent base chains and inherited property ids, classes that hold themselves inline and the wrong version.
 */

#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypeRegistryBinary.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include <variant>

namespace
{
    using Json = nlohmann::json;
    using OrderedJson = nlohmann::ordered_json;

    constexpr char const* LongString = "class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >";

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = 31, bool pointer = false)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    std::string Key(std::string const& name)
    {
        return std::to_string(StringHash::KiStringHash(name));
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[Key(name)] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    Json SyntheticDump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "enum TestMood", Json::array(), Json::object());
        AddClass(classes, "class Vector3D", Json::array(), Json::object());
        AddClass(classes, "unsigned int", Json::array(), Json::object());
        AddClass(classes, "class std::vector<int,class std::allocator<int> >", Json::array(), Json::object());
        AddClass(classes, "struct TestOpaqueEvent", Json::array(), Json::object());

        Json baseProperties = Json::object();
        baseProperties["m_id"] = Property("unsigned __int64", "m_id", 0);
        baseProperties["m_name"] = Property("std::string", "m_name", 1);
        AddClass(classes, "class TestBase", Json::array({ "PropertyClass" }), baseProperties);

        Json derivedProperties = Json::object();
        derivedProperties["m_position"] = Property("class Vector3D", "m_position", 2);
        derivedProperties["m_name"] = Property("std::string", "m_name", 1);
        derivedProperties["m_id"] = Property("unsigned __int64", "m_id", 0);
        Json mood = Property("enum TestMood", "m_mood", 3, "Static", 2097183);
        mood["enum_options"] = Json{ { "kCalm", 0 }, { "kAngry", 4 }, { "kNegative", -2 }, { "__DEFAULT", "kCalm" }, { "__BASECLASS", "TestMoodTable" }, { "Loud Voice", "Loud Voice" } };
        derivedProperties["m_mood"] = mood;
        derivedProperties["m_children"] = Property("class SharedPointer<class TestBase>", "m_children", 5, "Vector", 31, true);
        derivedProperties["m_flags"] = Property("bui5", "m_flags", 4);
        AddClass(classes, "class TestDerived", Json::array({ "TestBase", "PropertyClass" }), derivedProperties);

        AddClass(classes, "class TestDerived*", Json::array({ "TestBase", "PropertyClass" }), derivedProperties);
        AddClass(classes, "class SharedPointer<class TestBase>", Json::array({ "PropertyClass" }), baseProperties);

        Json lonelyProperties = Json::object();
        lonelyProperties["m_value"] = Property("float", "m_value", 0);
        AddClass(classes, "class TestAliasOnly*", Json::array({ "PropertyClass" }), lonelyProperties);

        Json templateProperties = Json::object();
        templateProperties["m_argument"] = Property("std::string", "m_argument", 0);
        AddClass(classes, "TestArgT<std::string>", Json::array({ "PropertyClass" }), templateProperties);
        AddClass(classes, std::string("class TestArgT<") + LongString + " >*", Json::array({ "PropertyClass" }), templateProperties);
        return Json{ { "version", 2 }, { "classes", classes } };
    }

    class TypeRegistryTest : public testing::Test
    {
    protected:
        std::vector<std::string> Refuse(std::string const& text)
        {
            EXPECT_FALSE(_registry.LoadFromText(text, "refused.json"));
            EXPECT_EQ(_registry.GetCatalog(), _active);
            return _registry.GetErrors();
        }

        void Activate()
        {
            ASSERT_TRUE(_registry.LoadFromText(SyntheticDump().dump(), "synthetic.json"));
            _active = _registry.GetCatalog();
        }

        TypeRegistry _registry;
        TypeCatalogPtr _active;
    };
}

TEST_F(TypeRegistryTest, AliasesCollapseAndBasesResolve)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    TypeCatalogPtr const catalog = _registry.GetCatalog();
    EXPECT_EQ(catalog->GetGeneration(), 1u);
    EXPECT_EQ(catalog->GetSha256().size(), 64u);

    ClassInfo const* const derived = catalog->FindClass("class TestDerived");
    ASSERT_NE(derived, nullptr);
    EXPECT_EQ(derived->Kind, ClassKind::PropertyClass);
    EXPECT_EQ(derived->Hash, StringHash::KiStringHash("class TestDerived"));
    EXPECT_EQ(catalog->FindClass("class TestDerived*"), derived);
    EXPECT_EQ(catalog->FindClass(StringHash::KiStringHash("class TestDerived*")), derived);
    EXPECT_EQ(catalog->FindClass(derived->Hash), derived);

    ClassInfo const* const base = catalog->FindClass("class TestBase");
    ClassInfo const* const root = catalog->FindClass("class PropertyClass");
    ASSERT_NE(base, nullptr);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(catalog->FindClass("class SharedPointer<class TestBase>"), base);
    ASSERT_EQ(derived->Bases.size(), 2u);
    EXPECT_EQ(derived->Bases[0], base);
    EXPECT_EQ(derived->Bases[1], root);
    EXPECT_TRUE(derived->IsA(*base));
    EXPECT_TRUE(derived->IsA(*root));
    EXPECT_TRUE(derived->IsA(*derived));
    EXPECT_FALSE(base->IsA(*derived));

    ClassInfo const* const lonely = catalog->FindClass("class TestAliasOnly");
    ASSERT_NE(lonely, nullptr);
    EXPECT_EQ(lonely->Hash, StringHash::KiStringHash("class TestAliasOnly"));
    EXPECT_EQ(catalog->FindClass("class TestAliasOnly*"), lonely);
    ASSERT_EQ(lonely->Properties.size(), 1u);
    EXPECT_EQ(lonely->Properties[0].Kind, ValueKind::Float);

    ClassInfo const* const argument = catalog->FindClass("TestArgT<std::string>");
    ASSERT_NE(argument, nullptr);
    EXPECT_EQ(argument->Hash, StringHash::KiStringHash("TestArgT<std::string>"));
    EXPECT_EQ(catalog->FindClass(std::string("class TestArgT<") + LongString + " >*"), argument);
    EXPECT_EQ(catalog->FindClass(std::string("class TestArgT<") + LongString + " >"), nullptr);

    EXPECT_EQ(catalog->GetAliasCount(), 4u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::PropertyClass), 5u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Enum), 1u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::ValueType), 1u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Primitive), 1u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Container), 1u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Opaque), 1u);
    EXPECT_EQ(catalog->GetClasses().size(), 10u);
    EXPECT_EQ(catalog->GetPropertyCount(), 10u);
    EXPECT_GT(catalog->GetApproximateBytes(), 0u);
    EXPECT_EQ(catalog->FindClass("class Missing"), nullptr);
    EXPECT_EQ(catalog->FindClass(uint32{ 12345 }), nullptr);
}

TEST(TypeRegistryBinaryTest, RoundTripsTheRegistryAndRejectsAnEditedPayload)
{
    std::filesystem::path const path = std::filesystem::temp_directory_path() / "ambrose-typeregistry-test.bin";
    std::string const text = SyntheticDump().dump();
    TypeDumpLoader::RawDump dump;
    std::vector<std::string> parseErrors;
    ASSERT_TRUE(TypeDumpLoader::Parse(text, dump, parseErrors));
    std::string error;
    ASSERT_TRUE(TypeRegistryBinary::Write(path, dump, "r-test", error)) << error;

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadBinary(path, "r-test")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_TRUE(catalog);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::PropertyClass), 5u);
    EXPECT_EQ(catalog->GetPropertyCount(), 10u);

    std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(stream);
    stream.seekp(-1, std::ios::end);
    char byte = '\0';
    stream.read(&byte, 1);
    stream.seekp(-1, std::ios::end);
    byte = static_cast<char>(byte ^ 0x01);
    stream.write(&byte, 1);
    stream.close();

    TypeRegistry stale;
    EXPECT_FALSE(stale.LoadBinary(path, "r-test"));
    ASSERT_FALSE(stale.GetErrors().empty());
    EXPECT_NE(stale.GetErrors().front().find("SHA-256"), std::string::npos);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(TypeRegistryBinaryTest, FallsBackToJsonForMissingOrStaleCaches)
{
    std::filesystem::path const directory = std::filesystem::temp_directory_path();
    std::filesystem::path const jsonPath = directory / "ambrose-typeregistry-fallback.json";
    std::filesystem::path const binaryPath = directory / "ambrose-typeregistry-fallback.bin";
    std::string const text = SyntheticDump().dump();
    {
        std::ofstream json(jsonPath, std::ios::binary | std::ios::trunc);
        json << text;
    }
    TypeDumpLoader::RawDump dump;
    std::vector<std::string> parseErrors;
    ASSERT_TRUE(TypeDumpLoader::Parse(text, dump, parseErrors));
    std::string error;
    ASSERT_TRUE(TypeRegistryBinary::Write(binaryPath, dump, "r-test", error)) << error;

    TypeRegistry stale;
    ASSERT_TRUE(stale.LoadBinary(binaryPath, jsonPath, "r-other"));
    EXPECT_EQ(stale.GetCatalog()->GetPropertyCount(), 10u);

    std::filesystem::remove(binaryPath);
    TypeRegistry missing;
    ASSERT_TRUE(missing.LoadBinary(binaryPath, jsonPath, "r-test"));
    EXPECT_EQ(missing.GetCatalog()->GetPropertyCount(), 10u);

    std::error_code ignored;
    std::filesystem::remove(jsonPath, ignored);
    std::filesystem::remove(binaryPath, ignored);
}

TEST(TypeRegistryBinaryTest, DirectCacheLoadIsFasterThanJsonForARepresentativeDump)
{
        Json large = SyntheticDump();
        for (uint32 index = 0; index != 1000; ++index)
        {
            std::string const name = "struct CacheBenchmark" + std::to_string(index);
            AddClass(large["classes"], name, Json::array(), Json::object());
        }
        std::string const text = large.dump();
        TypeDumpLoader::RawDump dump;
        std::vector<std::string> parseErrors;
        ASSERT_TRUE(TypeDumpLoader::Parse(text, dump, parseErrors));

        std::filesystem::path const jsonPath = std::filesystem::temp_directory_path() / "ambrose-typeregistry-benchmark.json";
        std::filesystem::path const binaryPath = std::filesystem::temp_directory_path() / "ambrose-typeregistry-benchmark.bin";
        {
            std::ofstream json(jsonPath, std::ios::binary | std::ios::trunc);
            json << text;
        }
        std::string error;
        ASSERT_TRUE(TypeRegistryBinary::Write(binaryPath, dump, "r-test", error)) << error;

        TypeRegistry jsonRegistry;
        TypeRegistry binaryRegistry;
        ASSERT_TRUE(jsonRegistry.LoadFromFile(jsonPath));
        ASSERT_TRUE(binaryRegistry.LoadBinary(binaryPath, "r-test"));

        auto const measure = [](auto&& load, int rounds)
        {
            bool succeeded = true;
            auto const start = std::chrono::steady_clock::now();
            for (int round = 0; round != rounds; ++round)
                succeeded = load() && succeeded;
            return std::pair{ std::chrono::steady_clock::now() - start, succeeded };
        };
        auto const jsonResult = measure([&] { return jsonRegistry.LoadFromFile(jsonPath); }, 3);
        auto const binaryResult = measure([&] { return binaryRegistry.LoadBinary(binaryPath, "r-test"); }, 3);
        ASSERT_TRUE(jsonResult.second);
        ASSERT_TRUE(binaryResult.second);
        EXPECT_LT(binaryResult.first, jsonResult.first) << "binary cache should be faster than JSON for the representative dump";

        std::error_code ignored;
        std::filesystem::remove(jsonPath, ignored);
        std::filesystem::remove(binaryPath, ignored);
}

TEST_F(TypeRegistryTest, PropertiesAreOrderedByIdAndClassified)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    TypeCatalogPtr const catalog = _registry.GetCatalog();
    ClassInfo const* const derived = catalog->FindClass("class TestDerived");
    ASSERT_NE(derived, nullptr);
    ASSERT_EQ(derived->Properties.size(), 6u);
    std::vector<std::string> names;
    for (PropertyInfo const& property : derived->Properties)
        names.push_back(property.Name);
    EXPECT_EQ(names, (std::vector<std::string>{ "m_id", "m_name", "m_position", "m_mood", "m_flags", "m_children" }));
    for (uint32 id = 0; id < derived->Properties.size(); ++id)
        EXPECT_EQ(derived->Properties[id].Id, id);

    EXPECT_EQ(derived->Properties[0].Kind, ValueKind::UInt64);
    EXPECT_EQ(derived->Properties[1].Kind, ValueKind::String);
    EXPECT_EQ(derived->Properties[2].Kind, ValueKind::Vector3D);
    EXPECT_EQ(derived->Properties[2].Type, catalog->FindClass("class Vector3D"));

    PropertyInfo const* const flags = derived->FindProperty("m_flags");
    ASSERT_NE(flags, nullptr);
    EXPECT_EQ(flags->Kind, ValueKind::UnsignedBits);
    EXPECT_EQ(flags->BitWidth, 5u);

    PropertyInfo const* const children = derived->FindProperty(StringHash::PropertyHash("class SharedPointer<class TestBase>", "m_children"));
    ASSERT_NE(children, nullptr);
    EXPECT_EQ(children->Name, "m_children");
    EXPECT_EQ(children->Kind, ValueKind::Object);
    EXPECT_EQ(children->Type, catalog->FindClass("class TestBase"));
    EXPECT_EQ(children->Container, ContainerKind::Vector);
    EXPECT_TRUE(children->Pointer);
    EXPECT_TRUE(children->Dynamic);
    EXPECT_EQ(derived->FindProperty("m_missing"), nullptr);
    EXPECT_EQ(derived->FindProperty(uint32{ 1 }), nullptr);
}

TEST_F(TypeRegistryTest, EnumOptionsLookUpInBothDirections)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    TypeCatalogPtr const catalog = _registry.GetCatalog();
    PropertyInfo const* const mood = catalog->FindClass("class TestDerived")->FindProperty("m_mood");
    ASSERT_NE(mood, nullptr);
    EXPECT_EQ(mood->Kind, ValueKind::Enum);
    EXPECT_EQ(mood->Type, catalog->FindClass("enum TestMood"));
    EXPECT_TRUE(mood->HasFlag(PropertyFlag::Enum));
    EXPECT_TRUE(mood->HasFlag(PropertyFlag::Transmit));
    EXPECT_FALSE(mood->HasFlag(PropertyFlag::Bits));
    EXPECT_EQ(mood->FindOptionValue("kAngry"), 4);
    EXPECT_EQ(mood->FindOptionValue("kNegative"), 4294967294);
    EXPECT_EQ(mood->FindOptionName(0), "kCalm");
    EXPECT_EQ(mood->FindOptionName(4294967294), "kNegative");
    EXPECT_FALSE(mood->FindOptionName(-2));
    EXPECT_FALSE(mood->FindOptionValue("kHappy"));
    EXPECT_FALSE(mood->FindOptionName(7));
    EXPECT_EQ(mood->Options.size(), 3u);
    ASSERT_TRUE(mood->Default);
    EXPECT_EQ(std::get<std::string>(*mood->Default), "kCalm");
    ASSERT_TRUE(mood->DefaultValue.Holds<int64>());
    EXPECT_EQ(*mood->DefaultValue.GetIf<int64>(), 0);
    EXPECT_EQ(mood->OptionBaseClass, "TestMoodTable");
    ASSERT_EQ(mood->TextOptions.size(), 1u);
    EXPECT_EQ(mood->TextOptions[0].Name, "Loud Voice");
    EXPECT_EQ(mood->TextOptions[0].Text, "Loud Voice");
}

TEST_F(TypeRegistryTest, DefaultsKeepTheirTypeWhereverTheyAppearAndDuplicateValuesKeepTheFirstName)
{
    OrderedJson axis = { { "type", "enum TestAxis" }, { "id", 0 }, { "offset", 8 }, { "flags", 31 }, { "container", "Static" }, { "dynamic", false }, { "singleton", false }, { "pointer", false },
        { "hash", StringHash::PropertyHash("enum TestAxis", "m_axis") } };
    axis["enum_options"] = OrderedJson{ { "__DEFAULT", 1 }, { "kX", 0 }, { "kY", 1 }, { "kAlsoY", 1 } };
    OrderedJson opacity = { { "type", "float" }, { "id", 1 }, { "offset", 16 }, { "flags", 31 }, { "container", "Static" }, { "dynamic", false }, { "singleton", false }, { "pointer", false },
        { "hash", StringHash::PropertyHash("float", "m_opacity") } };
    opacity["enum_options"] = OrderedJson{ { "__DEFAULT", "1.0" } };
    OrderedJson properties = OrderedJson::object();
    properties["m_axis"] = axis;
    properties["m_opacity"] = opacity;
    OrderedJson classes = OrderedJson::object();
    classes[Key("class PropertyClass")] = OrderedJson{ { "name", "class PropertyClass" }, { "bases", OrderedJson::array() }, { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", OrderedJson::object() } };
    classes[Key("enum TestAxis")] = OrderedJson{ { "name", "enum TestAxis" }, { "bases", OrderedJson::array() }, { "hash", StringHash::KiStringHash("enum TestAxis") }, { "properties", OrderedJson::object() } };
    classes[Key("class TestShape")] = OrderedJson{ { "name", "class TestShape" }, { "bases", OrderedJson::array({ "PropertyClass" }) }, { "hash", StringHash::KiStringHash("class TestShape") }, { "properties", properties } };
    std::string const text = OrderedJson{ { "version", 2 }, { "classes", classes } }.dump();
    ASSERT_LT(text.find("__DEFAULT"), text.find("kX"));

    ASSERT_TRUE(_registry.LoadFromText(text, "ordered.json")) << (_registry.GetErrors().empty() ? std::string() : _registry.GetErrors().front());
    ClassInfo const* const shape = _registry.GetCatalog()->FindClass("class TestShape");
    ASSERT_NE(shape, nullptr);
    PropertyInfo const& axisInfo = shape->Properties[0];
    ASSERT_EQ(axisInfo.Options.size(), 3u);
    ASSERT_TRUE(axisInfo.Default);
    EXPECT_EQ(std::get<int64>(*axisInfo.Default), 1);
    EXPECT_EQ(axisInfo.FindOptionName(1), "kY");
    EXPECT_EQ(axisInfo.FindOptionValue("kAlsoY"), 1);
    EXPECT_FALSE(axisInfo.FindOptionValue("__DEFAULT"));
    PropertyInfo const& opacityInfo = shape->Properties[1];
    EXPECT_TRUE(opacityInfo.Options.empty());
    ASSERT_TRUE(opacityInfo.Default);
    EXPECT_EQ(std::get<std::string>(*opacityInfo.Default), "1.0");
    ASSERT_TRUE(opacityInfo.DefaultValue.Holds<float>());
    EXPECT_EQ(*opacityInfo.DefaultValue.GetIf<float>(), 1.0f);
    ASSERT_TRUE(axisInfo.DefaultValue.Holds<int64>());
    EXPECT_EQ(*axisInfo.DefaultValue.GetIf<int64>(), 1);
}

TEST_F(TypeRegistryTest, EmptyOrMisshapenDumpsAreRefused)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    EXPECT_EQ(Refuse("{\"version\": 2}"), (std::vector<std::string>{ "the type dump has no classes object" }));
    EXPECT_EQ(Refuse("{\"version\": 2, \"classes\": {}}"), (std::vector<std::string>{ "the type dump lists no classes" }));
    EXPECT_EQ(Refuse("{\"version\": 2, \"classes\": []}"), (std::vector<std::string>{ "the type dump has an array as classes, which must be an object" }));
    EXPECT_EQ(Refuse("{\"version\": 2, \"classes\": \"x\"}"), (std::vector<std::string>{ "the type dump has a string as classes, which must be an object" }));
    EXPECT_EQ(Refuse("[1, 2]"), (std::vector<std::string>{ "the type dump is not a JSON object" }));
    EXPECT_EQ(Refuse("{\"version\": 2, \"classes\": {\"2\": 5}}"), (std::vector<std::string>{ "the classes object has a non-negative integer under key 2, where a class object must be" }));
    std::vector<std::string> const truncated = Refuse("{\"version\": 2, \"classes\": {");
    ASSERT_EQ(truncated.size(), 1u);
    EXPECT_NE(truncated[0].find("not valid JSON"), std::string::npos) << truncated[0];

    Json rootless = SyntheticDump();
    rootless["classes"].erase(Key("class PropertyClass"));
    std::vector<std::string> const errors = Refuse(rootless.dump());
    ASSERT_FALSE(errors.empty());
    EXPECT_EQ(errors[0], "the type dump has no class PropertyClass");

    Json wrongVersion = SyntheticDump();
    wrongVersion["version"] = 3;
    EXPECT_EQ(Refuse(wrongVersion.dump()), (std::vector<std::string>{ "the type dump is format version 3, but only version 2 is supported" }));

    EXPECT_FALSE(_registry.LoadFromFile("this-type-dump-does-not-exist.json"));
    EXPECT_EQ(_registry.GetCatalog(), _active);
}

TEST_F(TypeRegistryTest, KnownFieldsMustHaveTheirJsonTypeAndBePresent)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    std::string const baseKey = Key("class TestBase");
    std::string const derivedKey = Key("class TestDerived");

    Json basesObject = SyntheticDump();
    basesObject["classes"][baseKey]["bases"] = Json::object();
    EXPECT_EQ(Refuse(basesObject.dump()), (std::vector<std::string>{ "the class under key " + baseKey + " has an object as bases, which must be an array" }));

    Json pointerText = SyntheticDump();
    pointerText["classes"][derivedKey]["properties"]["m_children"]["pointer"] = "true";
    EXPECT_EQ(Refuse(pointerText.dump()), (std::vector<std::string>{ "class TestDerived property m_children has a string as pointer, which must be a boolean" }));

    Json optionsArray = SyntheticDump();
    optionsArray["classes"][derivedKey]["properties"]["m_mood"]["enum_options"] = Json::array({ 1, 2 });
    EXPECT_EQ(Refuse(optionsArray.dump()), (std::vector<std::string>{ "class TestDerived property m_mood has an array as enum_options, which must be an object" }));

    Json fractionalId = SyntheticDump();
    fractionalId["classes"][baseKey]["properties"]["m_id"]["id"] = 0.5;
    EXPECT_EQ(Refuse(fractionalId.dump()), (std::vector<std::string>{ "class TestBase property m_id has a fractional number as id, which must be a non-negative integer" }));

    Json nestedOption = SyntheticDump();
    nestedOption["classes"][derivedKey]["properties"]["m_mood"]["enum_options"]["kWeird"] = Json::object();
    EXPECT_EQ(Refuse(nestedOption.dump()), (std::vector<std::string>{ "class TestDerived property m_mood has an object as option kWeird, where an integer or text must be" }));

    Json missing = SyntheticDump();
    missing["classes"][baseKey]["properties"]["m_id"].erase("dynamic");
    missing["classes"][baseKey]["properties"]["m_id"].erase("pointer");
    EXPECT_EQ(Refuse(missing.dump()), (std::vector<std::string>{ "class TestBase property m_id has no dynamic, pointer" }));
}

TEST_F(TypeRegistryTest, StructuralProblemsAreAllReported)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    std::string const baseKey = Key("class TestBase");
    std::string const derivedKey = Key("class TestDerived");

    Json badHash = SyntheticDump();
    badHash["classes"][baseKey]["properties"]["m_name"]["hash"] = 7;
    badHash["classes"][baseKey]["hash"] = 8;
    badHash["classes"][derivedKey]["properties"]["m_position"]["container"] = "Map";
    badHash["classes"][derivedKey]["properties"]["m_flags"]["offset"] = uint64{ 1 } << 40;
    Json moved = badHash["classes"][Key("struct TestOpaqueEvent")];
    badHash["classes"].erase(Key("struct TestOpaqueEvent"));
    badHash["classes"]["17"] = moved;
    std::vector<std::string> reported = Refuse(badHash.dump());
    std::sort(reported.begin(), reported.end());
    std::vector<std::string> expected{
        "struct TestOpaqueEvent is listed under key 17 instead of its hash " + Key("struct TestOpaqueEvent"),
        "class TestBase records hash 8, but its name hashes to " + baseKey,
        "class TestBase property m_name records hash 7, but its type and name hash to " + std::to_string(StringHash::PropertyHash("std::string", "m_name")),
        "class TestDerived property m_flags has an id, offset or flags value above 32 bits",
        "class TestDerived property m_position has container Map, which is not Static, List or Vector" };
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(reported, expected);

    Json unknown = SyntheticDump();
    unknown["classes"][derivedKey]["bases"] = Json::array({ "TestMissingBase", "PropertyClass" });
    unknown["classes"][derivedKey]["properties"]["m_ghost"] = Property("class TestGhost", "m_ghost", 6);
    unknown["classes"][derivedKey]["properties"]["m_opaque"] = Property("struct TestOpaqueEvent", "m_opaque", 7);
    unknown["classes"][derivedKey]["properties"]["m_bits"] = Property("bf8", "m_bits", 8);
    EXPECT_EQ(Refuse(unknown.dump()), (std::vector<std::string>{
        "class TestDerived names base TestMissingBase, which the dump does not list",
        "class TestDerived property m_ghost has type class TestGhost, which the dump does not list",
        "class TestDerived property m_opaque has type struct TestOpaqueEvent, an opaque class that no value kind covers",
        "class TestDerived property m_bits has type bf8, which the dump does not list" }));

    Json gap = SyntheticDump();
    gap["classes"][baseKey]["properties"]["m_name"]["id"] = 2;
    EXPECT_EQ(Refuse(gap.dump()), (std::vector<std::string>{ "class TestBase property m_name has id 2, but property ids must run from 0 to 1",
        "class TestDerived lists class TestBase's property m_name with id 1 instead of 2, but views and compact data rely on inherited properties keeping their id" }));

    Json chain = SyntheticDump();
    chain["classes"][baseKey]["bases"] = Json::array();
    EXPECT_EQ(Refuse(chain.dump()), (std::vector<std::string>{ "class TestDerived lists 1 bases after class TestBase, but class TestBase itself lists 0" }));

    std::string text = SyntheticDump().dump();
    std::string const derivedEntry = "\"" + derivedKey + "\":";
    std::size_t const start = text.find(derivedEntry);
    ASSERT_NE(start, std::string::npos);
    std::size_t depth = 0;
    std::size_t end = text.find('{', start);
    for (; end < text.size(); ++end)
    {
        if (text[end] == '{')
            ++depth;
        else if (text[end] == '}' && --depth == 0)
            break;
    }
    std::string const duplicate = text.substr(start, end + 1 - start);
    text.insert(start, duplicate + ",");
    std::vector<std::string> const duplicated = Refuse(text);
    ASSERT_EQ(duplicated.size(), 1u);
    EXPECT_EQ(duplicated[0], "class TestDerived is listed twice");

    Json defaults = SyntheticDump();
    Json& derived = defaults["classes"][derivedKey]["properties"];
    derived["m_mood"]["enum_options"]["__DEFAULT"] = "kAngyr";
    derived["m_flags"]["enum_options"] = Json{ { "__DEFAULT", 32 } };
    derived["m_position"]["enum_options"] = Json{ { "__DEFAULT", "1,2,3" } };
    derived["m_children"]["enum_options"] = Json{ { "__DEFAULT", 0 } };
    defaults["classes"][baseKey]["properties"]["m_id"]["enum_options"] = Json{ { "__DEFAULT", "many" } };
    defaults["classes"][derivedKey]["properties"]["m_id"]["enum_options"] = Json{ { "__DEFAULT", "many" } };
    std::vector<std::string> refusedDefaults = Refuse(defaults.dump());
    std::sort(refusedDefaults.begin(), refusedDefaults.end());
    std::vector<std::string> expectedDefaults{
        "class TestBase property m_id has the default 'many', which does not resolve to a value of type unsigned __int64",
        "class TestDerived property m_children has the default 0, but a list property takes none",
        "class TestDerived property m_flags has the default 32, which does not resolve to a value of type bui5",
        "class TestDerived property m_id has the default 'many', which does not resolve to a value of type unsigned __int64",
        "class TestDerived property m_mood has the default 'kAngyr', which does not resolve to a value of type enum TestMood",
        "class TestDerived property m_position has the default '1,2,3', but a Vector3D property takes none" };
    std::sort(expectedDefaults.begin(), expectedDefaults.end());
    EXPECT_EQ(refusedDefaults, expectedDefaults);

    Json huge = SyntheticDump();
    huge["classes"][derivedKey]["properties"]["m_mood"]["enum_options"]["kHuge"] = int64{ 1 } << 33;
    EXPECT_EQ(Refuse(huge.dump()), (std::vector<std::string>{ "class TestDerived property m_mood has option kHuge with the value 8589934592, which does not fit 32 bits" }));

    Json self = SyntheticDump();
    Json nested = Json::object();
    nested["m_inner"] = Property("class TestNest", "m_inner", 0);
    AddClass(self["classes"], "class TestNest", Json::array({ "PropertyClass" }), nested);
    EXPECT_EQ(Refuse(self.dump()), (std::vector<std::string>{ "class TestNest holds itself inline through class TestNest.m_inner" }));

    Json loop = SyntheticDump();
    Json first = Json::object();
    first["m_id"] = Property("int", "m_id", 0);
    first["m_second"] = Property("class TestLoopB", "m_second", 1);
    AddClass(loop["classes"], "class TestLoopA", Json::array({ "PropertyClass" }), first);
    Json second = Json::object();
    second["m_first"] = Property("class TestLoopA", "m_first", 0);
    second["m_self"] = Property("class SharedPointer<class TestLoopB>", "m_self", 1, "Static", 31, true);
    second["m_many"] = Property("class TestLoopB", "m_many", 2, "List");
    AddClass(loop["classes"], "class TestLoopB", Json::array({ "PropertyClass" }), second);
    std::vector<std::string> const looped = Refuse(loop.dump());
    ASSERT_EQ(looped.size(), 1u);
    EXPECT_TRUE(looped[0] == "class TestLoopA holds itself inline through class TestLoopA.m_second -> class TestLoopB.m_first"
        || looped[0] == "class TestLoopB holds itself inline through class TestLoopB.m_first -> class TestLoopA.m_second") << looped[0];
}

TEST_F(TypeRegistryTest, ASuccessfulLoadStartsANewGenerationWhileOlderCatalogsStayUsable)
{
    ASSERT_NO_FATAL_FAILURE(Activate());
    ASSERT_TRUE(_registry.LoadFromText(SyntheticDump().dump(), "synthetic-again.json"));
    EXPECT_EQ(_registry.GetGeneration(), 2u);
    EXPECT_NE(_registry.GetCatalog(), _active);
    EXPECT_EQ(_active->GetGeneration(), 1u);
    EXPECT_NE(_active->FindClass("class TestDerived"), nullptr);
    _registry.Clear();
    EXPECT_FALSE(_registry.IsLoaded());
    EXPECT_EQ(_registry.GetGeneration(), 0u);
}
