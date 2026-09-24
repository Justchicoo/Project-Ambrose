/*
 * Project Ambrose by Imjustchico
 * Tests the plain-XML ObjectProperty reader on classes and documents the test invents: nested lists of derived and null objects, a skipped object kept as a null list entry, whitespace, comment-split and CDATA text kept, list elements checked one by one, an explicit inline object replacing its default in the limits, repeated and refused values reported accurately, enums, flag lists, bools, numbers, wide text, colors and vectors read into objects, a byte order mark skipped, unknown classes, unknown properties, unreadable values, repeated static properties and objects of the wrong class skipped and reported with their path and line, and documents that are not XML, have a second root or stray text beside it, are not an Objects document or break the depth, object and memory limits refused.
 */

#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlObjectReader.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = Saved, std::string container = "Static")
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
        for (char const* name : { "enum InputSource", "class Color", "class Vector3D" })
            add(name, Json::array(), Json::object());
        Json action = Json::object();
        action["m_name"] = Property("std::string", "m_name", 0);
        action["m_order"] = Property("int", "m_order", 1);
        action["m_usesVariants"] = Property("bool", "m_usesVariants", 2);
        Json source = Property("enum InputSource", "m_source", 3);
        source["enum_options"] = Json{ { "KBM", 0 }, { "Steam", 2 } };
        action["m_source"] = source;
        Json flags = Property("unsigned int", "m_flags", 4, Saved | (uint32{ 1 } << 20));
        flags["enum_options"] = Json{ { "FLAG_A", 1 }, { "FLAG_B", 2 } };
        action["m_flags"] = flags;
        action["m_label"] = Property("std::wstring", "m_label", 5);
        action["m_tint"] = Property("class Color", "m_tint", 6);
        action["m_offset"] = Property("class Vector3D", "m_offset", 7);
        action["m_id.m_full"] = Property("unsigned __int64", "m_id.m_full", 8);
        action["m_aliases"] = Property("std::string", "m_aliases", 9, Saved, "List");
        action["m_nibbles"] = Property("bui4", "m_nibbles", 10, Saved, "List");
        add("class TestAction", Json::array({ "PropertyClass" }), action);
        Json special = action;
        special["m_weight"] = Property("float", "m_weight", 11);
        add("class TestSpecialAction", Json::array({ "TestAction", "PropertyClass" }), special);
        Json list = Json::object();
        list["m_actions"] = Property("class TestAction*", "m_actions", 0, Saved, "List");
        list["m_default"] = Property("class TestAction", "m_default", 1);
        list["m_next"] = Property("class TestActionList*", "m_next", 2);
        add("class TestActionList", Json::array({ "PropertyClass" }), list);
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "xml.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    class XmlObjectReaderTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
        }

        TypeCatalogPtr _catalog;
    };
}

TEST_F(XmlObjectReaderTest, NestedListsEnumsAndValuesReadIntoObjects)
{
    std::string const document = "\xEF\xBB\xBF<Objects>\r\n"
        "  <Class Name=\"class TestActionList\">\r\n"
        "    <m_actions key=\"0\">\r\n"
        "      <Class Name=\"class TestSpecialAction\">\r\n"
        "        <m_name>ToggleTestPanel</m_name>\r\n"
        "        <m_order>-3</m_order>\r\n"
        "        <m_usesVariants>true</m_usesVariants>\r\n"
        "        <m_source>Steam</m_source>\r\n"
        "        <m_flags>FLAG_A|FLAG_B</m_flags>\r\n"
        "        <m_label>R\xC3\xA9sum\xC3\xA9</m_label>\r\n"
        "        <m_tint>FF1020C0</m_tint>\r\n"
        "        <m_offset>1.5, -2, 0.25</m_offset>\r\n"
        "        <m_id.m_full>71234</m_id.m_full>\r\n"
        "        <m_aliases key=\"0\">first</m_aliases>\r\n"
        "        <m_aliases key=\"1\">second &amp; third</m_aliases>\r\n"
        "        <m_weight>0.5</m_weight>\r\n"
        "      </Class>\r\n"
        "    </m_actions>\r\n"
        "    <m_actions key=\"1\"></m_actions>\r\n"
        "    <m_actions key=\"2\">\r\n"
        "      <Class Name=\"class TestAction\">\r\n"
        "        <m_name>Second</m_name>\r\n"
        "        <m_source>KBM</m_source>\r\n"
        "      </Class>\r\n"
        "    </m_actions>\r\n"
        "    <m_default>\r\n"
        "      <Class Name=\"class TestAction\"><m_order>9</m_order></Class>\r\n"
        "    </m_default>\r\n"
        "    <m_next>\r\n"
        "      <Class Name=\"class TestActionList\"></Class>\r\n"
        "    </m_next>\r\n"
        "  </Class>\r\n"
        "  <Class Name=\"class TestAction\"><m_name>Loose</m_name></Class>\r\n"
        "</Objects>\r\n";

    XmlReadResult const read = XmlObjectReader::Read(_catalog, document);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    EXPECT_TRUE(read.Issues.empty()) << read.Issues.front().Path << ' ' << read.Issues.front().Detail;
    ASSERT_EQ(read.Objects.size(), 2u);
    PropertyObject const& root = *read.Objects[0];
    EXPECT_EQ(root.GetClass().Name, "class TestActionList");
    PropertyValue::List const& actions = *root.Get("m_actions")->GetList();
    ASSERT_EQ(actions.size(), 3u);
    PropertyObject const* const special = actions[0].AsObject();
    ASSERT_NE(special, nullptr);
    EXPECT_TRUE(special->IsA("class TestSpecialAction"));
    EXPECT_EQ(*special->Get("m_name")->GetIf<std::string>(), "ToggleTestPanel");
    EXPECT_EQ(*special->Get("m_order")->GetIf<int32>(), -3);
    EXPECT_TRUE(*special->Get("m_usesVariants")->GetIf<bool>());
    EXPECT_EQ(*special->Get("m_source")->GetIf<int64>(), 2);
    EXPECT_EQ(*special->Get("m_flags")->GetIf<uint32>(), 3u);
    EXPECT_TRUE(*special->Get("m_label")->GetIf<std::u16string>() == u"R\u00e9sum\u00e9");
    EXPECT_TRUE(*special->Get("m_tint")->GetIf<PropertyTypes::Color>() == (PropertyTypes::Color{ 0x10, 0x20, 0xC0, 0xFF }));
    EXPECT_TRUE(*special->Get("m_offset")->GetIf<PropertyTypes::Vector3D>() == (PropertyTypes::Vector3D{ 1.5f, -2.0f, 0.25f }));
    EXPECT_EQ(*special->Get("m_id.m_full")->GetIf<uint64>(), 71234u);
    PropertyValue::List const& aliases = *special->Get("m_aliases")->GetList();
    ASSERT_EQ(aliases.size(), 2u);
    EXPECT_EQ(*aliases[1].GetIf<std::string>(), "second & third");
    EXPECT_EQ(*special->Get("m_weight")->GetIf<float>(), 0.5f);
    EXPECT_EQ(actions[1].AsObject(), nullptr);
    EXPECT_EQ(*actions[2].AsObject()->Get("m_name")->GetIf<std::string>(), "Second");
    EXPECT_EQ(*root.Get("m_default")->AsObject()->Get("m_order")->GetIf<int32>(), 9);
    ASSERT_NE(root.Get("m_next")->AsObject(), nullptr);
    EXPECT_TRUE(root.Get("m_next")->AsObject()->Get("m_actions")->GetList()->empty());
    EXPECT_EQ(*read.Objects[1]->Get("m_name")->GetIf<std::string>(), "Loose");
}

TEST_F(XmlObjectReaderTest, UnknownAndUnreadableContentIsSkippedAndReportedWithItsLine)
{
    std::string const document = "<Objects>\n"
        "  <Class Name=\"class Missing\"><m_name>x</m_name></Class>\n"
        "  <Class Name=\"class TestActionList\">\n"
        "    <m_actions><Class Name=\"class TestActionList\"></Class></m_actions>\n"
        "    <m_actions><Class Name=\"class Stranger\"></Class></m_actions>\n"
        "    <m_mystery>1</m_mystery>\n"
        "    <m_default><Class Name=\"class TestAction\">\n"
        "      <m_order>twelve</m_order>\n"
        "      <m_source>Gamepad</m_source>\n"
        "      <m_name>First</m_name>\n"
        "      <m_name>Last</m_name>\n"
        "      <m_tint>blue</m_tint>\n"
        "    </Class></m_default>\n"
        "  </Class>\n"
        "  <Stray/>\n"
        "</Objects>\n";

    XmlReadResult const read = XmlObjectReader::Read(_catalog, document);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    ASSERT_EQ(read.Objects.size(), 1u);
    PropertyObject const& root = *read.Objects[0];
    PropertyValue::List const& actions = *root.Get("m_actions")->GetList();
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].AsObject(), nullptr);
    EXPECT_EQ(actions[1].AsObject(), nullptr);
    PropertyObject const& action = *root.Get("m_default")->AsObject();
    EXPECT_EQ(*action.Get("m_order")->GetIf<int32>(), 0);
    EXPECT_EQ(*action.Get("m_source")->GetIf<int64>(), 0);
    EXPECT_EQ(*action.Get("m_name")->GetIf<std::string>(), "Last");

    ASSERT_EQ(read.Issues.size(), 9u);
    auto const expect = [&read](std::size_t index, DecodeIssueKind kind, std::string_view path, std::string_view detail)
    {
        EXPECT_EQ(read.Issues[index].Kind, kind) << index;
        EXPECT_EQ(read.Issues[index].Path, path) << index;
        EXPECT_EQ(read.Issues[index].Detail, detail) << index;
    };
    expect(0, DecodeIssueKind::UnknownClass, "Objects", "names class 'class Missing', which the type dump does not list on line 2");
    EXPECT_EQ(read.Issues[0].Hash, StringHash::KiStringHash("class Missing"));
    expect(1, DecodeIssueKind::InvalidObject, "class TestActionList.m_actions[0]", "holds a class TestActionList, which is not a class TestAction on line 4");
    expect(2, DecodeIssueKind::UnknownClass, "class TestActionList.m_actions[1]", "names class 'class Stranger', which the type dump does not list on line 5");
    expect(3, DecodeIssueKind::UnknownProperty, "class TestActionList", "holds <m_mystery>, which class TestActionList does not list on line 6");
    expect(4, DecodeIssueKind::InvalidValue, "class TestActionList.m_default.m_order", "holds 'twelve', which does not read as int on line 8");
    expect(5, DecodeIssueKind::UnknownEnumName, "class TestActionList.m_default.m_source", "holds 'Gamepad', which does not read as enum InputSource on line 9");
    expect(6, DecodeIssueKind::InvalidValue, "class TestActionList.m_default.m_name", "appears more than once; the last one is kept on line 11");
    expect(7, DecodeIssueKind::InvalidValue, "class TestActionList.m_default.m_tint", "holds 'blue', which does not read as class Color on line 12");
    expect(8, DecodeIssueKind::UnknownProperty, "Objects", "holds <Stray> where a Class element belongs on line 15");
}

TEST_F(XmlObjectReaderTest, ValuesKeepTheirTextAndListsCheckEachElement)
{
    std::string const document = "<Objects>\n"
        "  <Class Name=\"class TestAction\">\n"
        "    <m_name> </m_name>\n"
        "    <m_label>ab<!-- split -->cd<![CDATA[<ef>]]></m_label>\n"
        "    <m_order>7<Class Name=\"class TestAction\"/></m_order>\n"
        "    <m_nibbles>1</m_nibbles>\n"
        "    <m_nibbles>99</m_nibbles>\n"
        "    <m_nibbles>3</m_nibbles>\n"
        "    <m_usesVariants>true</m_usesVariants>\n"
        "    <m_usesVariants>maybe</m_usesVariants>\n"
        "    stray words\n"
        "  </Class>\n"
        "</Objects>\n";
    XmlReadResult const read = XmlObjectReader::Read(_catalog, document);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    ASSERT_EQ(read.Objects.size(), 1u);
    PropertyObject const& action = *read.Objects.front();
    EXPECT_EQ(*action.Get("m_name")->GetIf<std::string>(), " ");
    EXPECT_TRUE(*action.Get("m_label")->GetIf<std::u16string>() == u"abcd<ef>");
    EXPECT_EQ(*action.Get("m_order")->GetIf<int32>(), 0);
    PropertyValue::List const& nibbles = *action.Get("m_nibbles")->GetList();
    ASSERT_EQ(nibbles.size(), 2u);
    EXPECT_EQ(*nibbles[0].GetIf<uint32>(), 1u);
    EXPECT_EQ(*nibbles[1].GetIf<uint32>(), 3u);
    EXPECT_TRUE(*action.Get("m_usesVariants")->GetIf<bool>());

    ASSERT_EQ(read.Issues.size(), 4u);
    EXPECT_EQ(read.Issues[0].Path, "class TestAction.m_order");
    EXPECT_EQ(read.Issues[0].Detail, "holds the element <Class> where a int value belongs on line 5");
    EXPECT_EQ(read.Issues[1].Path, "class TestAction.m_nibbles[1]");
    EXPECT_EQ(read.Issues[1].Detail, "was refused and left out: the value does not fit the property's bit width or 32-bit enum range on line 7");
    EXPECT_EQ(read.Issues[2].Path, "class TestAction.m_usesVariants");
    EXPECT_EQ(read.Issues[2].Detail, "holds 'maybe', which does not read as bool on line 10");
    EXPECT_EQ(read.Issues[3].Detail, "holds text outside any property on line 11");
}

TEST_F(XmlObjectReaderTest, AnExplicitInlineObjectReplacesItsDefaultInTheLimits)
{
    std::string const document = "<Objects><Class Name=\"class TestActionList\"><m_default><Class Name=\"class TestAction\"/></m_default></Class></Objects>";
    SerializerLimits limits;
    limits.MaxObjects = 2;
    XmlReadResult const read = XmlObjectReader::Read(_catalog, document, limits);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    ASSERT_EQ(read.Objects.size(), 1u);
    limits.MaxObjects = 1;
    EXPECT_EQ(XmlObjectReader::Read(_catalog, document, limits).Status, XmlReadStatus::TooManyObjects);

    std::string const repeated = "<Objects><Class Name=\"class TestAction\"><m_nibbles>5</m_nibbles><m_order>4</m_order><m_order>x</m_order><m_order>6</m_order></Class></Objects>";
    XmlReadResult const twice = XmlObjectReader::Read(_catalog, repeated);
    ASSERT_TRUE(twice.Ok()) << twice.Detail;
    EXPECT_EQ(*twice.Objects.front()->Get("m_order")->GetIf<int32>(), 6);
    ASSERT_EQ(twice.Issues.size(), 2u);
    EXPECT_EQ(twice.Issues[0].Kind, DecodeIssueKind::InvalidValue);
    EXPECT_EQ(twice.Issues[0].Detail, "holds 'x', which does not read as int on line 1");
    EXPECT_EQ(twice.Issues[1].Detail, "appears more than once; the last one is kept on line 1");
}

TEST_F(XmlObjectReaderTest, DocumentsThatCannotBeReadAreRefused)
{
    XmlReadResult const broken = XmlObjectReader::Read(_catalog, "<Objects>\n<Class Name=\"class TestAction\">\n</Objects>");
    EXPECT_EQ(broken.Status, XmlReadStatus::BadXml);
    EXPECT_TRUE(broken.Objects.empty());
    EXPECT_NE(broken.Detail.find("is not well-formed XML"), std::string::npos) << broken.Detail;

    XmlReadResult const twoRoots = XmlObjectReader::Read(_catalog, "<Objects/>\n<Other/>");
    EXPECT_EQ(twoRoots.Status, XmlReadStatus::BadXml);
    EXPECT_EQ(twoRoots.Detail, "holds a second root element <Other> on line 2");
    XmlReadResult const trailing = XmlObjectReader::Read(_catalog, "<Objects/>\ngarbage");
    EXPECT_EQ(trailing.Status, XmlReadStatus::BadXml);
    SerializerLimits tiny;
    tiny.MaxDecodedBytes = 64;
    XmlReadResult const expensive = XmlObjectReader::Read(_catalog, "<Objects><Class Name=\"class TestAction\"/></Objects>", tiny);
    EXPECT_EQ(expensive.Status, XmlReadStatus::BudgetExceeded);
    EXPECT_NE(expensive.Detail.find("to parse"), std::string::npos) << expensive.Detail;

    XmlReadResult const wrongRoot = XmlObjectReader::Read(_catalog, "<Messages><Class Name=\"class TestAction\"/></Messages>");
    EXPECT_EQ(wrongRoot.Status, XmlReadStatus::NotObjects);
    EXPECT_EQ(wrongRoot.Detail, "has the root element <Messages> where <Objects> belongs");
    EXPECT_EQ(XmlObjectReader::Read(_catalog, "").Status, XmlReadStatus::BadXml);

    std::string deep = "<Objects>";
    for (int level = 0; level < 5; ++level)
        deep += "<Class Name=\"class TestActionList\"><m_next>";
    for (int level = 0; level < 5; ++level)
        deep += "</m_next></Class>";
    deep += "</Objects>";
    SerializerLimits shallow;
    shallow.MaxDepth = 4;
    XmlReadResult const tooDeep = XmlObjectReader::Read(_catalog, deep, shallow);
    EXPECT_EQ(tooDeep.Status, XmlReadStatus::TooDeep);
    EXPECT_TRUE(tooDeep.Objects.empty());
    EXPECT_TRUE(tooDeep.Issues.empty());
    shallow.MaxDepth = 5;
    EXPECT_EQ(XmlObjectReader::Read(_catalog, deep, shallow).Status, XmlReadStatus::TooDeep);
    shallow.MaxDepth = 6;
    EXPECT_TRUE(XmlObjectReader::Read(_catalog, deep, shallow).Ok());

    SerializerLimits few;
    few.MaxObjects = 8;
    EXPECT_EQ(XmlObjectReader::Read(_catalog, deep, few).Status, XmlReadStatus::TooManyObjects);
    SerializerLimits poor;
    poor.MaxDecodedBytes = 256;
    EXPECT_EQ(XmlObjectReader::Read(_catalog, deep, poor).Status, XmlReadStatus::BudgetExceeded);
    EXPECT_EQ(XmlObjectReader::GetStatusName(XmlReadStatus::TooDeep), "objects nest deeper than the limit");
}
