/*
 * Project Ambrose by Imjustchico
 * Tests the template manifest map: a plain path is an entry of Root.wad and a piped one an entry of its World-Part.wad, a world or part that is empty or could leave the GameData folder is refused, a manifest with an id of 0, an id listed twice, an empty path or a piped path missing a part is refused whole with every fault named, the report stops after its limit with a count of the rest, and a good manifest finds each id and lists its archives once each.
 */

#include "TemplateManifest.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }
}

TEST(TemplateManifestTest, APlainPathIsAnEntryOfRootWad)
{
    std::optional<TemplateLocation> const location = TemplateManifest::ParseLocation("ObjectData/Player/PlayerObject.xml");
    ASSERT_TRUE(location);
    EXPECT_EQ(location->Archive, "Root.wad");
    EXPECT_EQ(location->Path, "ObjectData/Player/PlayerObject.xml");
}

TEST(TemplateManifestTest, APipedPathIsAnEntryOfItsWorldPartArchive)
{
    std::optional<TemplateLocation> const location = TemplateManifest::ParseLocation("|Krokotopia|WorldData|ObjectData/Mummy.xml");
    ASSERT_TRUE(location);
    EXPECT_EQ(location->Archive, "Krokotopia-WorldData.wad");
    EXPECT_EQ(location->Path, "ObjectData/Mummy.xml");
    EXPECT_EQ(TemplateManifest::ParseLocation("|WizardCity|WC_Streets|a|b.xml"), (TemplateLocation{ "WizardCity-WC_Streets.wad", "a|b.xml" })) << "a pipe past the part belongs to the entry";
}

TEST(TemplateManifestTest, APathThatNamesNoArchiveAndEntryIsRefused)
{
    std::vector<std::string> const paths{ "", "|", "|Krokotopia", "|Krokotopia|", "|Krokotopia|WorldData", "|Krokotopia|WorldData|", "||WorldData|a.xml", "|Krokotopia||a.xml",
        "|..|WorldData|a.xml", "|.|WorldData|a.xml", "|Kroko/topia|WorldData|a.xml", "|Krokotopia|World\\Data|a.xml", "|C:|WorldData|a.xml", "|Kroko\ttopia|WorldData|a.xml" };
    for (std::string const& path : paths)
        EXPECT_FALSE(TemplateManifest::ParseLocation(path)) << path;
}

TEST(TemplateManifestTest, AGoodManifestFindsEachIdAndListsItsArchivesOnce)
{
    std::vector<std::string> errors;
    std::shared_ptr<TemplateManifest const> const manifest = TemplateManifest::Build(
        { { 1, "ObjectData/Player.xml" }, { 7, "|Krokotopia|WorldData|ObjectData/Mummy.xml" }, { 8, "|Krokotopia|WorldData|ObjectData/Scarab.xml" }, { 9, "|Grizzleheim|WorldData|ObjectData/Bear.xml" } }, errors);
    ASSERT_TRUE(manifest) << errors.front();
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(manifest->Size(), 4u);
    ASSERT_NE(manifest->Find(8), nullptr);
    EXPECT_EQ(*manifest->Find(8), (TemplateLocation{ "Krokotopia-WorldData.wad", "ObjectData/Scarab.xml" }));
    EXPECT_EQ(manifest->Find(2), nullptr);
    EXPECT_EQ(manifest->GetArchives(), (std::vector<std::string>{ "Grizzleheim-WorldData.wad", "Krokotopia-WorldData.wad", "Root.wad" }));
}

TEST(TemplateManifestTest, AManifestWithFaultsIsRefusedWholeWithEveryFaultNamed)
{
    std::vector<std::string> errors;
    std::shared_ptr<TemplateManifest const> const manifest = TemplateManifest::Build(
        { { 1, "ObjectData/Player.xml" }, { 0, "ObjectData/Zero.xml" }, { 1, "ObjectData/Again.xml" }, { 5, "" }, { 6, "|Krokotopia|ObjectData/Mummy.xml" } }, errors);
    EXPECT_FALSE(manifest);
    ASSERT_EQ(errors.size(), 4u);
    EXPECT_TRUE(Holds(errors, "TemplateManifest.xml lists template 0 at ObjectData/Zero.xml, and 0 names no template"));
    EXPECT_TRUE(Holds(errors, "TemplateManifest.xml lists template 1 twice"));
    EXPECT_TRUE(Holds(errors, "TemplateManifest.xml gives template 5 no path"));
    EXPECT_TRUE(Holds(errors, "TemplateManifest.xml gives template 6 the path |Krokotopia|ObjectData/Mummy.xml, which names no archive and entry"));
}

TEST(TemplateManifestTest, TheReportStopsAfterItsLimitWithACountOfTheRest)
{
    std::vector<std::pair<uint32, std::string>> entries;
    for (uint32 id = 1; id <= TemplateManifest::MaxReportedErrors + 25; ++id)
        entries.emplace_back(id, "");
    std::vector<std::string> errors;
    EXPECT_FALSE(TemplateManifest::Build(entries, errors));
    ASSERT_EQ(errors.size(), TemplateManifest::MaxReportedErrors + 1);
    EXPECT_EQ(errors.back(), "and 25 more problems");
}

TEST(TemplateManifestTest, AnEmptyManifestFindsNothing)
{
    std::vector<std::string> errors;
    std::shared_ptr<TemplateManifest const> const manifest = TemplateManifest::Build({}, errors);
    ASSERT_TRUE(manifest);
    EXPECT_EQ(manifest->Size(), 0u);
    EXPECT_EQ(manifest->Find(1), nullptr);
    EXPECT_TRUE(manifest->GetArchives().empty());
}
