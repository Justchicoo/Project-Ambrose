/*
 * Project Ambrose by Imjustchico
 * Loads the locale files of the user's own r806919 install, when AMBROSE_CLIENT_DIR names it: en-US holds 5132 files, 217032 entries and 216992 keys and resolves the item, quest title, zone, quest goal and NPC format keys the roadmap names, every installed locale loads with Polish skipping only its malformed WizardFurniture.lang, the German item text differs from the English, and when AMBROSE_TYPE_DUMP_PATH names the type dump every display name of the first 2000 object templates in Root.wad resolves or is reported as missing, with at most one percent missing.
 */

#include "BindFile.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LocaleStore.h"
#include "LogConfig.h"
#include "ObjectViews.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t TemplateSample = 2000;

    class LocaleStoreClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!client || client->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own r806919 install to run this test";
            std::string error;
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(archive) << error;
            _archive = std::move(archive);
            ASSERT_TRUE(_store.Load(_archive, "en-US", error)) << error;
        }

        std::shared_ptr<KiwadArchive const> _archive;
        LocaleStore _store;
    };
}

TEST_F(LocaleStoreClientTest, EnglishLoadsEveryTableAndResolvesTheNamedKeys)
{
    std::shared_ptr<LocaleTable const> const english = _store.GetTable();
    ASSERT_TRUE(english);
    std::cout << "[ LOCALE   ] en-US: " << english->GetFileCount() << " files, " << english->GetEntryCount() << " entries, " << english->GetKeyCount() << " keys, "
              << english->GetDuplicateCount() << " repeated; locales:";
    for (std::string const& locale : _store.GetLocales())
        std::cout << ' ' << locale;
    std::cout << std::endl;
    EXPECT_EQ(english->GetFileCount(), 5132u);
    EXPECT_EQ(english->GetEntryCount(), 217032u);
    EXPECT_EQ(english->GetKeyCount(), 216992u);
    EXPECT_EQ(english->GetDuplicateCount(), 40u);
    EXPECT_TRUE(english->GetProblems().empty());
    EXPECT_EQ(_store.Resolve("Items_00028316"), "Cute Fairy Kei Broadbrim");
    EXPECT_EQ(_store.Resolve("QuestTitle_00001718"), "To Ravenwood!");
    EXPECT_EQ(_store.Resolve("ZoneLocName_1451497"), "Wizard City|Ravenwood");
    EXPECT_EQ(_store.Resolve("WizardQuestGoals_TalkNPC"), "Talk To");
    std::optional<std::string> const format = _store.Resolve("NPCFormats_Name");
    ASSERT_TRUE(format);
    EXPECT_NE(format->find("$NPC_NAME$"), std::string::npos) << *format;
    std::vector<std::string> const found = english->FindKeys("To Ravenwood!");
    EXPECT_NE(std::find(found.begin(), found.end(), "QuestTitle_00001718"), found.end());

    std::optional<std::string> const german = _store.Resolve("Items_00028316", "de");
    ASSERT_TRUE(german);
    EXPECT_FALSE(german->empty());
    EXPECT_NE(*german, "Cute Fairy Kei Broadbrim");
}

TEST_F(LocaleStoreClientTest, EveryInstalledLocaleLoads)
{
    std::vector<std::string> const locales = _store.GetLocales();
    EXPECT_GE(locales.size(), 7u);
    for (std::string const& locale : locales)
    {
        std::string error;
        std::shared_ptr<LocaleTable const> const table = _store.GetTable(locale, &error);
        ASSERT_TRUE(table) << error;
        std::cout << "[ LOCALE   ] " << locale << ": " << table->GetFileCount() << " files, " << table->GetKeyCount() << " keys, " << table->GetProblems().size() << " skipped" << std::endl;
        EXPECT_GT(table->GetKeyCount(), 0u) << locale;
        if (locale == "pl")
        {
            ASSERT_EQ(table->GetProblems().size(), 1u);
            EXPECT_NE(table->GetProblems().front().find("Locale/pl/WizardFurniture.lang"), std::string::npos) << table->GetProblems().front();
        }
        else
        {
            EXPECT_TRUE(table->GetProblems().empty()) << locale << ": " << table->GetProblems().front();
        }
    }
}

TEST_F(LocaleStoreClientTest, EveryDisplayNameInTheTemplateSampleResolvesOrIsReported)
{
    std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!dump || dump->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump to run this test";
    TypeRegistry registry(&sTypedViewRegistry);
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*dump)));
    TypeCatalogPtr const catalog = registry.GetCatalog();

    std::size_t sampled = 0;
    std::size_t resolved = 0;
    std::vector<std::string> missing;
    for (KiwadEntry const& entry : _archive->GetEntries())
    {
        if (sampled == TemplateSample)
            break;
        if (!entry.Name.starts_with("ObjectData/"))
            continue;
        KiwadReadResult const read = _archive->Read(entry);
        if (!read.Succeeded() || !BindFile::IsBind(read.Data))
            continue;
        BindReadResult const file = BindFile::Read(catalog, read.Data);
        if (!file.Ok())
            continue;
        std::optional<GameObjectTemplateView> const view = GameObjectTemplateView::From(*file.Decoded.Object);
        if (!view)
            continue;
        std::string const& name = view->GetDisplayName();
        ++sampled;
        if (name.empty() || _store.HasKey(name))
            ++resolved;
        else
            missing.push_back(entry.Name + ": " + name);
    }
    std::cout << "[ LOCALE   ] " << sampled << " templates sampled, " << resolved << " display names resolve or are empty, " << missing.size() << " missing" << std::endl;
    for (std::string const& name : missing)
        std::cout << "[ MISSING  ] " << name << std::endl;
    EXPECT_EQ(sampled, TemplateSample);
    EXPECT_EQ(resolved + missing.size(), sampled);
    EXPECT_LE(missing.size(), sampled / 100);
}
