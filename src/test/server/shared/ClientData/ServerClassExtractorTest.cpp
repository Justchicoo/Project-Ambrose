/*
 * Project Ambrose by Imjustchico
 * Tests the server class extractor over an archive written by the test with a catalog that knows two behaviors, swept by a registry that knows neither: the behavior the program names is kept once its list property, first read as one value, is tried again as a list, the extraction ends on the sweep that proves it, with the unknown classes and their objects counted before and after, and the behavior no program string names stays unknown with its reason; and over plain-XML object files, a class only they hold is kept with the classes it points to and a derived one with the base its dump holder declares, the files then read cleanly, while a class a BINd file holds stays judged by that file.
 */

#include "BindFile.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "PropertyObject.h"
#include "ServerClassExtractor.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlObjectReader.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", bool pointer = false)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 7 }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::string Dump(bool withBehaviors)
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        Json behavior = Json::object();
        behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
        Json object = Json::object();
        object["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "Vector", true);
        AddClass(classes, "class GameObjectTemplate", Json::array({ "PropertyClass" }), object);
        if (withBehaviors)
        {
            Json deck = behavior;
            deck["m_spellList"] = Property("unsigned int", "m_spellList", 1, "List");
            AddClass(classes, "class DeckBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }), deck);
            Json hidden = behavior;
            hidden["m_secret"] = Property("unsigned int", "m_secret", 1);
            AddClass(classes, "class HiddenBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }), hidden);
        }
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }
}

TEST(ServerClassExtractorTest, TheNamedBehaviorIsKeptOnceItsListDecodesAndTheUnnamedOneStaysUnknown)
{
    TypeRegistry full;
    ASSERT_TRUE(full.LoadFromText(Dump(true), "full.json"));
    TypeCatalogPtr const writing = full.GetCatalog();
    KiwadBuilder builder(2);
    for (uint32 copy = 0; copy < 5; ++copy)
    {
        PropertyObjectPtr deck = PropertyObject::Create(writing, "class DeckBehaviorTemplate");
        ASSERT_EQ(deck->Set("m_behaviorName", "MobDeck"), PropertySetResult::Ok);
        ASSERT_EQ(deck->Set("m_spellList", PropertyValue::List{ PropertyValue(uint32{ 7 }), PropertyValue(uint32{ 8 + copy }) }), PropertySetResult::Ok);
        PropertyObjectPtr hidden = PropertyObject::Create(writing, "class HiddenBehaviorTemplate");
        ASSERT_EQ(hidden->Set("m_behaviorName", "Hidden"), PropertySetResult::Ok);
        ASSERT_EQ(hidden->Set("m_secret", uint32{ copy }), PropertySetResult::Ok);
        PropertyObjectPtr root = PropertyObject::Create(writing, "class GameObjectTemplate");
        PropertyValue::List behaviors;
        behaviors.emplace_back(std::move(deck));
        behaviors.emplace_back(std::move(hidden));
        ASSERT_EQ(root->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
        EncodeResult const written = BindFile::Write(root.get());
        ASSERT_TRUE(written.Ok()) << written.Detail;
        builder.Add("ObjectData/Mob" + std::to_string(copy) + ".xml", written.Bytes, copy % 2 == 0);
    }
    LogTestDirectory directory;
    std::filesystem::path const path = directory.Path() / "Root.wad";
    std::vector<uint8> const archiveBytes = builder.Build();
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(archiveBytes.data()), static_cast<std::streamsize>(archiveBytes.size()));
    }
    TypeRegistry served;
    ASSERT_TRUE(served.LoadFromText(Dump(false), "served.json"));
    PropertyOracle const oracle(*served.GetCatalog(), { "m_spellList", "m_secret" }, { "unsigned int" });
    ServerClassNames const names{ { StringHash::KiStringHash("class DeckBehaviorTemplate"), { "class DeckBehaviorTemplate" } } };
    std::vector<uint32> rounds;
    ServerClassExtractorOptions options;
    options.Threads = 2;
    options.Progress = [&rounds](uint32 round, std::size_t) { rounds.push_back(round); };
    ServerClassExtraction const found = ServerClassExtractor::Run({ path }, served, {}, oracle, names, options);

    ASSERT_TRUE(found.Ok()) << found.Errors.front();
    ASSERT_EQ(found.Classes.size(), 1u);
    TypeDumpLoader::RawClass const& deck = found.Classes.front().Class;
    EXPECT_EQ(*deck.Name, "class DeckBehaviorTemplate");
    ASSERT_EQ(deck.Properties.size(), 2u);
    EXPECT_EQ(*deck.Properties[1].Container, "List") << "read first as one value, the list raised a size mismatch and was tried again as a list";
    EXPECT_EQ(found.Rounds, 4u) << "propose, find the mismatch, propose the list, prove it";
    EXPECT_EQ(rounds, (std::vector<uint32>{ 1, 2, 3 }));
    EXPECT_EQ(found.UnknownBefore, 2u);
    EXPECT_EQ(found.ObjectsBefore, 10u);
    EXPECT_EQ(found.UnknownAfter, 1u);
    EXPECT_EQ(found.ObjectsAfter, 5u);
    EXPECT_EQ(found.FailuresBefore, 0u);
    EXPECT_EQ(found.FailuresAfter, 0u);
    ASSERT_EQ(found.Refused.size(), 1u);
    EXPECT_EQ(found.Refused.front().Hash, StringHash::KiStringHash("class HiddenBehaviorTemplate"));
    EXPECT_EQ(found.Refused.front().Reason, "no string of the client program hashes to it");
    EXPECT_TRUE(served.IsFromSupplement(StringHash::KiStringHash("class DeckBehaviorTemplate"))) << "the registry ends holding what was kept";
}

TEST(ServerClassExtractorTest, ClassesOnlyTheXmlFilesHoldAreKeptOnceTheyReadCleanlyAndOneABindFileHoldsIsLeftToIt)
{
    TypeRegistry full;
    ASSERT_TRUE(full.LoadFromText(Dump(true), "full.json"));
    TypeCatalogPtr const writing = full.GetCatalog();
    KiwadBuilder builder(2);
    PropertyObjectPtr hidden = PropertyObject::Create(writing, "class HiddenBehaviorTemplate");
    ASSERT_EQ(hidden->Set("m_behaviorName", "Hidden"), PropertySetResult::Ok);
    PropertyObjectPtr root = PropertyObject::Create(writing, "class GameObjectTemplate");
    PropertyValue::List behaviors;
    behaviors.emplace_back(std::move(hidden));
    ASSERT_EQ(root->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
    EncodeResult const written = BindFile::Write(root.get());
    ASSERT_TRUE(written.Ok()) << written.Detail;
    builder.Add("ObjectData/Hidden.xml", written.Bytes, true);
    std::string const colors = "<Objects>\n"
                               "  <Class Name=\"class ShoppingColors\">\n"
                               "    <m_boysPrimary>FF0808AC</m_boysPrimary>\n"
                               "    <m_boysPrimary>FF0061CE</m_boysPrimary>\n"
                               "    <m_palette><Class Name=\"class ColorSwatch\"><m_label>Warm</m_label></Class></m_palette>\n"
                               "  </Class>\n"
                               "</Objects>\n";
    builder.Add("Colors.xml", std::string_view(colors), true);
    builder.Add("ObjectData/Placed.xml", std::string_view("<Objects><Class Name=\"class GameObjectTemplate\"><m_behaviors><Class Name=\"class ExtraBehaviorTemplate\">"
                                                          "<m_behaviorName>Extra</m_behaviorName><m_extra>7</m_extra></Class></m_behaviors></Class></Objects>"), false);
    builder.Add("ObjectData/HiddenPlaced.xml", std::string_view("<Objects><Class Name=\"class GameObjectTemplate\"><m_behaviors><Class Name=\"class HiddenBehaviorTemplate\">"
                                                                "<m_behaviorName>Hidden</m_behaviorName><m_secret>3</m_secret></Class></m_behaviors></Class></Objects>"), false);
    LogTestDirectory directory;
    std::filesystem::path const path = directory.Path() / "Root.wad";
    std::vector<uint8> const archiveBytes = builder.Build();
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(archiveBytes.data()), static_cast<std::streamsize>(archiveBytes.size()));
    }
    TypeRegistry served;
    ASSERT_TRUE(served.LoadFromText(Dump(false), "served.json"));
    PropertyOracle const oracle(*served.GetCatalog());
    ServerClassExtraction const found = ServerClassExtractor::Run({ path }, served, {}, oracle, {});

    ASSERT_TRUE(found.Ok()) << found.Errors.front();
    std::vector<std::string> kept;
    for (ServerClassProposal const& proposal : found.Classes)
        kept.push_back(*proposal.Class.Name);
    std::sort(kept.begin(), kept.end());
    EXPECT_EQ(kept, (std::vector<std::string>{ "class ColorSwatch", "class ExtraBehaviorTemplate", "class ShoppingColors" }));
    ASSERT_EQ(found.Refused.size(), 1u);
    EXPECT_EQ(found.Refused.front().Hash, StringHash::KiStringHash("class HiddenBehaviorTemplate"));
    EXPECT_EQ(found.Refused.front().Reason, "no string of the client program hashes to it") << "a class a BINd file holds is judged from it, whatever the XML files say";
    EXPECT_EQ(found.XmlDocuments, 3u);
    EXPECT_EQ(found.XmlUnknownBefore, 4u);
    EXPECT_EQ(found.XmlObjectsBefore, 4u);
    EXPECT_EQ(found.XmlUnknownAfter, 1u);
    EXPECT_EQ(found.XmlObjectsAfter, 1u);
    EXPECT_EQ(found.XmlFailuresBefore, 0u);
    EXPECT_EQ(found.XmlFailuresAfter, 0u);
    EXPECT_EQ(found.Rounds, 2u) << "propose, then the sweep that reads every file cleanly";

    XmlReadResult const read = XmlObjectReader::Read(served.GetCatalog(), colors);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    EXPECT_TRUE(read.Issues.empty()) << read.Issues.front().Detail;
    ASSERT_EQ(read.Objects.size(), 1u);
    EXPECT_EQ(read.Objects.front()->GetClass().Name, "class ShoppingColors");
    ASSERT_NE(read.Objects.front()->Get("m_boysPrimary")->GetList(), nullptr);
    EXPECT_EQ(read.Objects.front()->Get("m_boysPrimary")->GetList()->size(), 2u);
    PropertyObject const* const swatch = read.Objects.front()->Get("m_palette")->AsObject();
    ASSERT_NE(swatch, nullptr) << "a class the files hold points to another they hold";
    EXPECT_EQ(*swatch->Get("m_label")->GetIf<std::string>(), "Warm");
}
