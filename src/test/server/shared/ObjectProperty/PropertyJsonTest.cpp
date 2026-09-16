/*
 * Project Ambrose by Imjustchico
 * Tests the JSON form of property objects on classes the test invents: the class key first and properties in id order, enum and flag values by name or number, wide text as UTF-8, value types as arrays, lists, a derived child and a null one, text holding bytes that are not UTF-8 repaired, and NaN and infinities spelled out.
 */

#include "PropertyJson.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <string>

namespace
{
    using Json = nlohmann::json;
    using OrderedJson = nlohmann::ordered_json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = Wire, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*');
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        for (char const* name : { "enum JsonMood", "class Color", "class Vector3D" })
            add(name, Json::array(), Json::object());
        Json node = Json::object();
        node["m_name"] = Property("std::string", "m_name", 0);
        node["m_title"] = Property("std::wstring", "m_title", 1);
        Json mood = Property("enum JsonMood", "m_mood", 2);
        mood["enum_options"] = Json{ { "kCalm", 0 }, { "kAngry", 4 } };
        node["m_mood"] = mood;
        Json mask = Property("unsigned int", "m_mask", 3, Wire | (uint32{ 1 } << 20));
        mask["enum_options"] = Json{ { "FLAG_A", 1 }, { "FLAG_B", 2 } };
        node["m_mask"] = mask;
        node["m_tint"] = Property("class Color", "m_tint", 4);
        node["m_position"] = Property("class Vector3D", "m_position", 5);
        node["m_scores"] = Property("int", "m_scores", 6, Wire, "List");
        node["m_children"] = Property("class JsonNode*", "m_children", 7, Wire, "List");
        node["m_ready"] = Property("bool", "m_ready", 8);
        add("class JsonNode", Json::array({ "PropertyClass" }), node);
        Json leaf = node;
        leaf["m_weight"] = Property("double", "m_weight", 9);
        add("class JsonLeaf", Json::array({ "JsonNode", "PropertyClass" }), leaf);
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "json.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }
}

TEST(PropertyJsonTest, AnObjectBecomesOrderedJsonWithNamesAndNestedChildren)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr const root = PropertyObject::Create(catalog, "class JsonNode");
    PropertyObjectPtr const leaf = PropertyObject::Create(catalog, "class JsonLeaf");
    ASSERT_TRUE(root && leaf);
    ASSERT_EQ(root->Set("m_name", "Root"), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_title", u"R\u00e9sum\u00e9"), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_mood", int64{ 4 }), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_mask", uint32{ 3 }), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_tint", PropertyTypes::Color{ 10, 20, 30, 255 }), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_position", PropertyTypes::Vector3D{ 1.5f, -2.0f, 0.0f }), PropertySetResult::Ok);
    PropertyValue::List scores;
    scores.emplace_back(int32{ 7 });
    scores.emplace_back(int32{ -1 });
    ASSERT_EQ(root->Set("m_scores", std::move(scores)), PropertySetResult::Ok);
    ASSERT_EQ(leaf->Set("m_mask", uint32{ 12 }), PropertySetResult::Ok);
    ASSERT_EQ(leaf->Set("m_weight", 0.25), PropertySetResult::Ok);
    PropertyValue::List children;
    children.emplace_back(leaf->Clone());
    children.emplace_back(PropertyObjectPtr());
    ASSERT_EQ(root->Set("m_children", std::move(children)), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_ready", true), PropertySetResult::Ok);

    OrderedJson const json = PropertyJson::ToJson(root.get());
    OrderedJson const expected = OrderedJson::parse(R"({
        "$class": "class JsonNode",
        "m_name": "Root",
        "m_title": "R\u00e9sum\u00e9",
        "m_mood": "kAngry",
        "m_mask": "FLAG_A|FLAG_B",
        "m_tint": [10, 20, 30, 255],
        "m_position": [1.5, -2.0, 0.0],
        "m_scores": [7, -1],
        "m_children": [
            {
                "$class": "class JsonLeaf",
                "m_name": "",
                "m_title": "",
                "m_mood": "kCalm",
                "m_mask": 12,
                "m_tint": [0, 0, 0, 0],
                "m_position": [0.0, 0.0, 0.0],
                "m_scores": [],
                "m_children": [],
                "m_ready": false,
                "m_weight": 0.25
            },
            null
        ],
        "m_ready": true
    })");
    EXPECT_EQ(json, expected) << json.dump(2);
    EXPECT_EQ(json.begin().key(), "$class");
    EXPECT_TRUE(PropertyJson::ToJson(nullptr).is_null());

    ASSERT_EQ(root->Set("m_name", std::string("bad \xff byte")), PropertySetResult::Ok);
    ASSERT_EQ(leaf->Set("m_weight", std::numeric_limits<double>::quiet_NaN()), PropertySetResult::Ok);
    ASSERT_EQ(root->Set("m_position", PropertyTypes::Vector3D{ std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 0.0f }), PropertySetResult::Ok);
    OrderedJson const repaired = PropertyJson::ToJson(root.get());
    EXPECT_EQ(repaired["m_name"], "bad \xEF\xBF\xBD byte");
    EXPECT_EQ(repaired["m_position"], OrderedJson::parse(R"(["Infinity", "-Infinity", 0.0])"));
    EXPECT_EQ(PropertyJson::ToJson(leaf.get())["m_weight"], "NaN");
    EXPECT_NO_THROW(repaired.dump());
    std::string const text = PropertyJson::Dump(root.get(), -1);
    EXPECT_NE(text.find("\"m_name\":\"bad \xEF\xBF\xBD byte\""), std::string::npos) << text;
    EXPECT_EQ(text.find('\n'), std::string::npos);
}
