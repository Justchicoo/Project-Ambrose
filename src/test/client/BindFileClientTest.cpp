/*
 * Project Ambrose by Imjustchico
 * Reads BINd files from the user's own r806919 install, when AMBROSE_CLIENT_DIR names it and AMBROSE_TYPE_DUMP_PATH its type dump: the compressed TemplateManifest.xml holds 137423 template locations starting with ObjectData/PlayerObject.xml, a Series 58 crown hat decodes through the typed views to template 1652259 with its display name, three jewel sockets and its school and level requirements, and a sweep of every Root.wad BINd file decodes all but those whose root class the dump does not list, with no size mismatch or other issue, and prints the unknown class report; the same sweep lists every property an object of an unknown class holds, the property oracle names the NPC behavior 520243970's m_behaviorName, m_npcProximity, m_questList and m_personaName from the dump's own types and names, and unknown result and requirement classes are found under m_results and m_requirements.
 */

#include "BindSweep.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "ObjectViews.h"
#include "PropertyOracle.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::size_t PrintedClasses = 25;

    class BindFileClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own r806919 install and AMBROSE_TYPE_DUMP_PATH to its type dump to run this test";
            _registry = std::make_unique<TypeRegistry>(&sTypedViewRegistry);
            ASSERT_TRUE(_registry->LoadFromFile(LogConfig::Utf8Path(*dump))) << (_registry->GetErrors().empty() ? std::string() : _registry->GetErrors().front());
            _catalog = _registry->GetCatalog();
            std::string error;
            _archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(_archive) << error;
        }

        BindReadResult ReadEntry(std::string_view name)
        {
            KiwadReadResult const read = _archive->Read(name);
            EXPECT_TRUE(read.Succeeded()) << name << ": " << read.Error;
            return BindFile::Read(_catalog, read.Data);
        }

        BindSweepReport const& Sweep()
        {
            if (!s_report)
                s_report = BindSweep::Run(*_archive, _catalog);
            return *s_report;
        }

        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        std::unique_ptr<KiwadArchive> _archive;
        static inline std::optional<BindSweepReport> s_report;
    };
}

TEST_F(BindFileClientTest, TheTemplateManifestListsEveryTemplateLocation)
{
    BindReadResult const manifest = ReadEntry("TemplateManifest.xml");
    ASSERT_TRUE(manifest.Ok()) << manifest.Detail;
    EXPECT_TRUE((manifest.Flags & SerializerFlag::Compress) != SerializerFlag::None);
    EXPECT_TRUE(manifest.Decoded.Issues.empty());
    std::optional<TemplateManifestView> const view = TemplateManifestView::From(*manifest.Decoded.Object);
    ASSERT_TRUE(view);
    PropertyValue::List const& locations = view->GetSerializedTemplates();
    ASSERT_EQ(locations.size(), 137423u);
    std::optional<TemplateLocationView> const first = TemplateLocationView::From(*locations.front().AsObject());
    ASSERT_TRUE(first);
    EXPECT_EQ(first->GetFilename(), "ObjectData/PlayerObject.xml");
    EXPECT_EQ(first->GetId(), 1u);
}

TEST_F(BindFileClientTest, ASeries58CrownHatDecodesThroughItsViews)
{
    BindReadResult const hat = ReadEntry("ObjectData/CrownItems/Series58/Hats/Crowns-S58-Hats-L110-BS-008-01.xml");
    ASSERT_TRUE(hat.Ok()) << hat.Detail;
    EXPECT_TRUE(hat.Decoded.Issues.empty());
    PropertyObject const& item = *hat.Decoded.Object;
    std::optional<WizItemTemplateView> const view = WizItemTemplateView::From(item);
    ASSERT_TRUE(view);
    EXPECT_EQ(view->GetTemplateId(), 1652259u);
    std::optional<GameObjectTemplateView> const object = GameObjectTemplateView::From(item);
    ASSERT_TRUE(object);
    EXPECT_EQ(object->GetDisplayName(), "Items_00028316");

    PropertyValue::List const& behaviors = object->GetBehaviors();
    auto const sockets = std::find_if(behaviors.begin(), behaviors.end(), [](PropertyValue const& behavior)
    {
        return behavior.AsObject() && behavior.AsObject()->IsA("class JewelSocketBehaviorTemplate");
    });
    ASSERT_NE(sockets, behaviors.end());
    EXPECT_EQ(sockets->AsObject()->Get("m_jewelSockets")->GetList()->size(), 3u);

    PropertyObject const* const requirements = view->GetEquipRequirements();
    ASSERT_NE(requirements, nullptr);
    std::optional<RequirementListView> const list = RequirementListView::From(*requirements);
    ASSERT_TRUE(list);
    PropertyValue::List const& entries = list->GetRequirements();
    ASSERT_EQ(entries.size(), 2u);
    ASSERT_TRUE(entries[0].AsObject() && entries[0].AsObject()->IsA("class ReqSchoolOfFocus"));
    EXPECT_EQ(*entries[0].AsObject()->Get("m_magicSchool")->GetIf<std::string>(), "Balance");
    ASSERT_TRUE(entries[1].AsObject() && entries[1].AsObject()->IsA("class ReqMagicLevel"));
    EXPECT_EQ(*entries[1].AsObject()->Get("m_numericValue")->GetIf<float>(), 110.0f);
}

TEST_F(BindFileClientTest, EveryRootWadBindFileDecodesOrNamesItsUnknownRootClass)
{
    BindSweepReport const& report = Sweep();
    std::cout << "[ SWEEP    ] " << report.Entries << " entries, " << report.Files << " BINd files, " << report.Decoded << " decoded, " << report.Failures.size() << " failed, "
              << report.UnknownClasses.size() << " unknown classes" << std::endl;
    for (std::size_t index = 0; index < report.UnknownClasses.size() && index < PrintedClasses; ++index)
    {
        BindSweepUnknownClass const& unknown = report.UnknownClasses[index];
        std::cout << "[ UNKNOWN  ] class hash " << unknown.Hash << " x" << unknown.Count << " in " << unknown.Files << " file(s), first in " << unknown.FirstFile << " at " << unknown.FirstPath << std::endl;
    }
    for (BindSweepFailure const& failure : report.Failures)
        std::cout << "[ FAILED   ] " << failure.File << ": " << failure.Detail << std::endl;

    EXPECT_EQ(report.Files, 134640u);
    EXPECT_EQ(report.ReadErrors, 0u);
    EXPECT_EQ(report.Decoded + report.Failures.size(), report.Files);
    for (BindSweepFailure const& failure : report.Failures)
    {
        EXPECT_EQ(failure.DecodeStatus, SerializerStatus::UnknownClass) << failure.File << ": " << failure.Detail;
        EXPECT_NE(failure.RootClassHash, 0u) << failure.File;
    }
    EXPECT_LE(report.Failures.size(), 5u);
    for (BindSweepIssue const& issue : report.Issues)
        ADD_FAILURE() << ObjectSerializer::GetIssueName(issue.Kind) << " x" << issue.Count << ", first in " << issue.FirstFile << " at " << issue.FirstPath << ": " << issue.FirstDetail;
}

TEST_F(BindFileClientTest, EachPropertyOfAnUnknownClassIsListedAndTheOracleNamesTheNpcBehaviors)
{
    BindSweepReport const& report = Sweep();
    constexpr uint32 NpcBehavior = 520243970;
    auto const unknown = std::find_if(report.UnknownClasses.begin(), report.UnknownClasses.end(), [](BindSweepUnknownClass const& item) { return item.Hash == NpcBehavior; });
    ASSERT_NE(unknown, report.UnknownClasses.end());
    EXPECT_EQ(unknown->Count, 6875u);

    PropertyOracle const oracle(*_catalog);
    std::vector<std::string> named;
    std::size_t listed = 0;
    for (BindSweepClassProperty const& property : report.ClassProperties)
    {
        if (property.Owner != NpcBehavior)
            continue;
        ++listed;
        EXPECT_EQ(property.Count, 6875u) << "every NPC behavior holds property " << property.Hash;
        for (PropertyGuess const& guess : oracle.Guess(property.Hash))
            named.push_back(guess.Type + ":" + guess.Name);
    }
    EXPECT_EQ(listed, 5u);
    for (std::string_view const expected : { "std::string:m_behaviorName", "float:m_npcProximity", "std::string:m_questList", "std::string:m_personaName" })
        EXPECT_NE(std::find(named.begin(), named.end(), expected), named.end()) << expected << " is not among " << named.size() << " guesses";

    auto const under = [&report](std::string_view list)
    {
        return std::any_of(report.UnknownClasses.begin(), report.UnknownClasses.end(), [list](BindSweepUnknownClass const& item) { return item.FirstPath.find(list) != std::string::npos; });
    };
    EXPECT_TRUE(under(".m_results[")) << "unknown result classes sit under ResultList.m_results";
    EXPECT_TRUE(under(".m_requirements[")) << "unknown requirement classes sit under RequirementList.m_requirements";
}
