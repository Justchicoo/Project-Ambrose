/*
 * Project Ambrose by Imjustchico
 * Tests the XML sweep over an archive written by the test: only entries named .xml that are Objects documents are read, a byte order mark, declaration and comment before the root included and BINd files and other roots passed over; a document that is not well-formed is refused by name; each class the catalog does not describe is reported in the order first seen with each element its objects hold, whether one repeats it or writes it with a key, the classes it holds, its distinct values and how often it is empty, and the class and property holding it; an unknown property of a known class is an issue owned by that class; a sweep limited to named entries reads only them; merging adds the counts; and the test for an Objects document looks past what may come before the root.
 */

#include "BindFile.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlSweep.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
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

    constexpr char const* Chatter = "\xEF\xBB\xBF<Objects>\n"
                                    "  <Class Name=\"class ChatterManager\">\n"
                                    "    <m_loadItems key=\"0\">\n"
                                    "      <Class Name=\"class Chatter\">\n"
                                    "        <m_id.m_full>84099</m_id.m_full>\n"
                                    "        <m_chatterList>Chatter_0</m_chatterList>\n"
                                    "        <m_name>Chat1</m_name>\n"
                                    "      </Class>\n"
                                    "    </m_loadItems>\n"
                                    "    <m_loadItems key=\"1\">\n"
                                    "      <Class Name=\"class Chatter\">\n"
                                    "        <m_id.m_full>84100</m_id.m_full>\n"
                                    "        <m_chatterList key=\"0\">Chatter_1</m_chatterList>\n"
                                    "        <m_chatterList key=\"1\">Chatter_2</m_chatterList>\n"
                                    "        <m_name></m_name>\n"
                                    "      </Class>\n"
                                    "    </m_loadItems>\n"
                                    "  </Class>\n"
                                    "</Objects>\n";

    constexpr char const* Holder = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                   "<!-- placed by hand -->\n"
                                   "<Objects>\n"
                                   "  <Class Name=\"class GameObjectTemplate\">\n"
                                   "    <m_behaviors>\n"
                                   "      <Class Name=\"class ExtraBehaviorTemplate\">\n"
                                   "        <m_behaviorName>Extra</m_behaviorName>\n"
                                   "        <m_extra>7</m_extra>\n"
                                   "      </Class>\n"
                                   "    </m_behaviors>\n"
                                   "    <m_unlisted>1</m_unlisted>\n"
                                   "  </Class>\n"
                                   "</Objects>\n";

    class XmlSweepTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Json classes = Json::object();
            AddClass(classes, "class PropertyClass", Json::array(), Json::object());
            Json behavior = Json::object();
            behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
            AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
            Json object = Json::object();
            object["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "Vector", true);
            AddClass(classes, "class GameObjectTemplate", Json::array({ "PropertyClass" }), object);
            ASSERT_TRUE(_registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "sweep.json"));
            _catalog = _registry.GetCatalog();

            PropertyObjectPtr bound = PropertyObject::Create(_catalog, "class BehaviorTemplate");
            ASSERT_EQ(bound->Set("m_behaviorName", "Bound"), PropertySetResult::Ok);
            EncodeResult const written = BindFile::Write(bound.get());
            ASSERT_TRUE(written.Ok()) << written.Detail;

            KiwadBuilder builder(2);
            builder.Add("Chatter.xml", std::string_view(Chatter), true);
            builder.Add("ObjectData/Holder.xml", std::string_view(Holder), false);
            builder.Add("Known.xml", std::string_view("<Objects><Class Name=\"class BehaviorTemplate\"><m_behaviorName>Plain</m_behaviorName></Class></Objects>"), false);
            builder.Add("Broken.xml", std::string_view("<Objects><Class Name=\"class BehaviorTemplate\"></Objects>"), false);
            builder.Add("Settings.xml", std::string_view("<Config><Value>1</Value></Config>"), false);
            builder.Add("Bound.xml", written.Bytes, true);
            builder.Add("Notes.txt", std::string_view("<Objects></Objects>"), false);
            _path = _directory.Path() / "Root.wad";
            std::vector<uint8> const bytes = builder.Build();
            std::ofstream stream(_path, std::ios::binary);
            stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        std::unique_ptr<KiwadArchive> Open()
        {
            std::string error;
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(_path, error);
            EXPECT_TRUE(archive) << error;
            return archive;
        }

        static XmlSweepProperty const* Element(XmlSweepClass const& seen, std::string const& name)
        {
            auto const found = std::find_if(seen.Properties.begin(), seen.Properties.end(), [&name](XmlSweepProperty const& property) { return property.Name == name; });
            return found == seen.Properties.end() ? nullptr : &*found;
        }

        TypeRegistry _registry;
        TypeCatalogPtr _catalog;
        LogTestDirectory _directory;
        std::filesystem::path _path;
    };
}

TEST_F(XmlSweepTest, EveryObjectsDocumentIsReadAndTheClassesTheCatalogLacksAreDescribedFromTheirElements)
{
    std::unique_ptr<KiwadArchive> const archive = Open();
    ASSERT_TRUE(archive);
    XmlSweepReport const report = XmlSweep::Run(*archive, _catalog);

    EXPECT_EQ(report.Entries, 6u) << "every entry named .xml, and not Notes.txt";
    EXPECT_EQ(report.Documents, 4u) << "not Settings.xml, whose root is not Objects, nor the BINd file";
    EXPECT_EQ(report.Read, 3u);
    EXPECT_EQ(report.ReadErrors, 0u);
    ASSERT_EQ(report.Failures.size(), 1u);
    EXPECT_EQ(report.Failures.front().File, "Broken.xml");
    EXPECT_EQ(report.Failures.front().Status, XmlReadStatus::BadXml);
    EXPECT_EQ(std::set<std::string>(report.UnknownFiles.begin(), report.UnknownFiles.end()), (std::set<std::string>{ "Chatter.xml", "ObjectData/Holder.xml" }));

    ASSERT_EQ(report.UnknownClasses.size(), 3u);
    XmlSweepClass const& manager = report.UnknownClasses[0];
    EXPECT_EQ(manager.Name, "class ChatterManager");
    EXPECT_TRUE(manager.AtRoot);
    EXPECT_TRUE(manager.Holders.empty());
    EXPECT_EQ(manager.Count, 1u);
    EXPECT_EQ(manager.FirstFile, "Chatter.xml");
    XmlSweepProperty const* const items = Element(manager, "m_loadItems");
    ASSERT_NE(items, nullptr);
    EXPECT_EQ(items->Count, 2u);
    EXPECT_TRUE(items->Repeats);
    EXPECT_TRUE(items->Keyed);
    EXPECT_FALSE(items->Mixed) << "blank space beside an object is not text";
    EXPECT_EQ(items->Held, (std::set<std::string>{ "class Chatter" }));
    EXPECT_TRUE(items->Values.empty());

    XmlSweepClass const& chatter = report.UnknownClasses[1];
    EXPECT_EQ(chatter.Name, "class Chatter");
    EXPECT_FALSE(chatter.AtRoot);
    EXPECT_EQ(chatter.Holders, (std::set<std::pair<std::string, std::string>>{ { "class ChatterManager", "m_loadItems" } }));
    EXPECT_EQ(chatter.Count, 2u);
    EXPECT_EQ(chatter.Files, 1u);
    EXPECT_EQ(chatter.FirstPath, "class ChatterManager.m_loadItems[0]");
    ASSERT_EQ(chatter.Properties.size(), 3u);
    EXPECT_EQ(chatter.Properties[0].Name, "m_id.m_full") << "elements keep the order they are first seen in";
    EXPECT_EQ(chatter.Properties[0].Values, (std::set<std::string>{ "84099", "84100" }));
    XmlSweepProperty const* const lines = Element(chatter, "m_chatterList");
    ASSERT_NE(lines, nullptr);
    EXPECT_EQ(lines->Count, 3u);
    EXPECT_TRUE(lines->Repeats) << "the second chatter repeats it";
    EXPECT_TRUE(lines->Keyed);
    XmlSweepProperty const* const name = Element(chatter, "m_name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->Values, (std::set<std::string>{ "Chat1" }));
    EXPECT_EQ(name->Empty, 1u);
    EXPECT_FALSE(name->Repeats);
    EXPECT_FALSE(name->Keyed);

    XmlSweepClass const& extra = report.UnknownClasses[2];
    EXPECT_EQ(extra.Name, "class ExtraBehaviorTemplate");
    EXPECT_EQ(extra.Holders, (std::set<std::pair<std::string, std::string>>{ { "class GameObjectTemplate", "m_behaviors" } }));
    ASSERT_EQ(extra.Properties.size(), 2u);
    EXPECT_EQ(extra.Properties[0].Name, "m_behaviorName");
    EXPECT_EQ(extra.Properties[1].Values, (std::set<std::string>{ "7" }));

    ASSERT_EQ(report.Issues.size(), 1u);
    EXPECT_EQ(report.Issues.front().Kind, DecodeIssueKind::UnknownProperty);
    EXPECT_EQ(report.Issues.front().Owner, StringHash::KiStringHash("class GameObjectTemplate"));
    EXPECT_EQ(report.Issues.front().FirstFile, "ObjectData/Holder.xml");
    EXPECT_EQ(report.Issues.front().Count, 1u);
}

TEST_F(XmlSweepTest, ASweepLimitedToNamedEntriesReadsOnlyThemAndMergingAddsTheCounts)
{
    std::unique_ptr<KiwadArchive> const archive = Open();
    ASSERT_TRUE(archive);
    std::vector<std::string> const only{ "ObjectData/Holder.xml" };
    XmlSweepReport limited = XmlSweep::Run(*archive, _catalog, &only);
    EXPECT_EQ(limited.Entries, 1u);
    EXPECT_EQ(limited.Documents, 1u);
    ASSERT_EQ(limited.UnknownClasses.size(), 1u);
    EXPECT_EQ(limited.UnknownClasses.front().Name, "class ExtraBehaviorTemplate");

    XmlSweepReport merged = XmlSweep::Run(*archive, _catalog);
    XmlSweep::Merge(merged, std::move(limited));
    EXPECT_EQ(merged.Entries, 7u);
    EXPECT_EQ(merged.Documents, 5u);
    ASSERT_EQ(merged.UnknownClasses.size(), 3u) << "a class both reports hold is joined, not repeated";
    EXPECT_EQ(merged.UnknownClasses[2].Count, 2u);
    EXPECT_EQ(merged.UnknownClasses[2].Files, 2u);
    EXPECT_EQ(merged.UnknownClasses[2].FirstFile, "ObjectData/Holder.xml");
    ASSERT_EQ(merged.Issues.size(), 1u);
    EXPECT_EQ(merged.Issues.front().Count, 2u);
    EXPECT_EQ(merged.UnknownFiles.size(), 3u);
}

TEST_F(XmlSweepTest, AnObjectsDocumentIsKnownByItsFirstElementWhateverComesBeforeIt)
{
    EXPECT_TRUE(XmlSweep::IsObjectsDocument("<Objects>"));
    EXPECT_TRUE(XmlSweep::IsObjectsDocument("<Objects/>"));
    EXPECT_TRUE(XmlSweep::IsObjectsDocument("\xEF\xBB\xBF \r\n<?xml version=\"1.0\"?><!-- a <Config> -->\n<Objects version=\"1\">"));
    EXPECT_FALSE(XmlSweep::IsObjectsDocument("<ObjectsList>"));
    EXPECT_FALSE(XmlSweep::IsObjectsDocument("<Config><Objects/></Config>"));
    EXPECT_FALSE(XmlSweep::IsObjectsDocument("<!-- never closed <Objects>"));
    EXPECT_FALSE(XmlSweep::IsObjectsDocument("<Objects"));
    EXPECT_FALSE(XmlSweep::IsObjectsDocument(""));
}
