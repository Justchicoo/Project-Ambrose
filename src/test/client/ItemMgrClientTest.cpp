/*
 * Project Ambrose by Imjustchico
 * Reads every item template of the user's own install through the item manager, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it, with the classes the install holds beside the dump as the game server reads them: every template under ObjectData/ decodes and every item among them loads with no requirement or effect of a class neither describes, as many WizItemTemplates as recorded for the installed revision, r806919's 76679, printed with the other item classes, the memory they take and how long they took; the hat 1652259 is an item under the display key Items_00028316; and in a scratch folder holding a copy of the install's Root.wad with the hat's cost re-encoded, beside read-only links to the other archives that hold ObjectData, the item_template reload swaps in the edited hat under a new generation, and with the install's classes taken away a reload fails naming the class hash it met and keeps the edited set serving.
 */

#include "BindFile.h"
#include "Environment.h"
#include "InstalledClasses.h"
#include "InstalledRevision.h"
#include "ItemMgr.h"
#include "KiwadArchive.h"
#include "KiwadPatcher.h"
#include "LogTestDirectory.h"
#include "LogConfig.h"
#include "ObjectTemplateMgr.h"
#include "ReloadMgr.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    class ItemMgrClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            TypeDumpLoader::RawDump classes;
            std::string source;
            std::string error;
            ASSERT_TRUE(InstalledClasses::Read(classes, source, error)) << error;
            std::vector<std::string> errors;
            ASSERT_TRUE(sTypeRegistry.SetSupplement(std::move(classes), source, errors)) << (errors.empty() ? std::string() : errors.front());
            ASSERT_TRUE(sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(*dump)));
            sObjectTemplateMgr.SetInstall(LogConfig::Utf8Path(*client));
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            s_items = std::make_unique<ItemMgr>();
            s_items->SetInstall(LogConfig::Utf8Path(*client));
            auto const started = std::chrono::steady_clock::now();
            s_loaded = s_items->Load(errors);
            s_took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
            s_errors = errors;
        }

        static void TearDownTestSuite()
        {
            s_items.reset();
            sObjectTemplateMgr.Clear();
            std::vector<std::string> errors;
            sTypeRegistry.ClearSupplement(errors);
            sTypeRegistry.Clear();
        }

        void SetUp() override
        {
            if (!s_items)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            for (std::string const& error : s_errors)
                std::cout << error << "\n";
            ASSERT_TRUE(s_loaded) << (s_errors.empty() ? std::string("the items did not load") : s_errors.front());
        }

        static inline std::unique_ptr<ItemMgr> s_items;
        static inline bool s_loaded = false;
        static inline std::vector<std::string> s_errors;
        static inline std::chrono::milliseconds s_took{ 0 };
    };
}

TEST_F(ItemMgrClientTest, EveryItemTemplateLoadsWithNoFailure)
{
    std::shared_ptr<ItemTemplateStore const> const items = s_items->GetItems();
    std::map<std::string, std::size_t> const classes = items->CountByClass();
    for (auto const& [name, count] : classes)
        std::cout << fmt::format("{} {}\n", count, name);
    std::cout << fmt::format("{} item templates, {} behaviors of classes nothing describes, {:.1f} MiB, read in {} ms\n", items->Size(), items->CountUnknownBehaviors(),
        static_cast<double>(items->GetMemoryUsage()) / (1024.0 * 1024.0), s_took.count());
    auto const plain = classes.find(std::string(ItemTemplateRecord::ItemClass));
    InstalledRevision::Expect(plain == classes.end() ? std::size_t{ 0 } : plain->second, { { "r806919", std::size_t{ 76679 } } }, "WizItemTemplates");
}

TEST_F(ItemMgrClientTest, TheHatIsAnItemUnderItsDisplayKey)
{
    ItemTemplateRecord const* const hat = s_items->GetItems()->Find(1652259);
    ASSERT_NE(hat, nullptr);
    EXPECT_EQ(hat->ClassName, std::string(ItemTemplateRecord::ItemClass));
    EXPECT_EQ(hat->DisplayKey, "Items_00028316");
    std::cout << fmt::format("{} {} {}, school '{}', cost {}, rank {}, limit {}, set bonus {}\n", hat->TemplateId, hat->ObjectName, hat->File, hat->School, hat->BaseCost, hat->Rank,
        hat->ItemLimit, hat->ItemSetBonusTemplateId);
}

TEST_F(ItemMgrClientTest, AnItemEditedInACopyOfTheInstallAppliesOnReloadAndAReloadMeetingAClassTheDumpLacksKeepsIt)
{
    constexpr uint32 hatId = 1652259;
    constexpr float editedCost = 4242.0f;
    std::filesystem::path const client = LogConfig::Utf8Path(*Ambrose::GetEnv("AMBROSE_CLIENT_DIR"));
    LogTestDirectory scratch;
    std::filesystem::path const gameData = scratch.Path() / "Data" / "GameData";
    std::filesystem::create_directories(gameData);
    std::set<std::string> archives;
    for (auto const& [id, place] : sObjectTemplateMgr.GetManifest()->GetLocations())
        if (place.Path.starts_with(ItemMgr::Folder))
            archives.insert(place.Archive);
    TemplateLocation const* const location = sObjectTemplateMgr.GetManifest()->Find(hatId);
    ASSERT_NE(location, nullptr);
    ASSERT_EQ(location->Archive, "Root.wad");
    for (std::string const& archive : archives)
    {
        std::filesystem::path const from = client / "Data" / "GameData" / LogConfig::Utf8Path(archive);
        std::filesystem::path const to = gameData / LogConfig::Utf8Path(archive);
        std::error_code linked;
        if (archive != location->Archive)
            std::filesystem::create_hard_link(from, to, linked);
        if (archive == location->Archive || linked)
            std::filesystem::copy_file(from, to);
    }

    sReloadMgr.Clear();
    ItemMgr items;
    items.SetInstall(scratch.Path());
    items.RegisterReloadTargets();
    ReloadOutcome const first = sReloadMgr.Reload(ItemMgr::Target);
    ASSERT_TRUE(first.Ok) << (first.Errors.empty() ? std::string() : first.Errors.front());
    std::shared_ptr<ItemTemplateStore const> const held = items.GetItems();
    uint64 const generation = items.GetGeneration();
    float const cost = held->Find(hatId)->BaseCost;
    ASSERT_NE(cost, editedCost);

    std::vector<uint8> edited;
    {
        std::string error;
        std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(gameData / "Root.wad", error);
        ASSERT_NE(root, nullptr) << error;
        KiwadReadResult const bytes = root->Read(location->Path);
        ASSERT_TRUE(bytes.Succeeded()) << bytes.Error;
        BindReadResult const read = BindFile::Read(sTypeRegistry.GetCatalog(), bytes.Data);
        ASSERT_TRUE(read.Ok() && read.Decoded.Object) << read.Detail;
        ASSERT_EQ(read.Decoded.Object->Set("m_baseCost", PropertyValue(editedCost)), PropertySetResult::Ok);
        EncodeResult const encoded = BindFile::Write(read.Decoded.Object.get(), read.Flags);
        ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
        edited = encoded.Bytes;
    }
    std::string error;
    ASSERT_TRUE(KiwadPatcher::Replace(gameData / "Root.wad", location->Path, edited, error)) << error;

    ReloadOutcome const reloaded = sReloadMgr.Reload(ItemMgr::Target);
    for (std::string const& line : ReloadMgr::Describe(reloaded))
        std::cout << line << "\n";
    ASSERT_TRUE(reloaded.Ok) << (reloaded.Errors.empty() ? std::string() : reloaded.Errors.front());
    EXPECT_GT(items.GetGeneration(), generation);
    EXPECT_EQ(items.GetItems()->Size(), held->Size());
    EXPECT_FLOAT_EQ(items.GetItems()->Find(hatId)->BaseCost, editedCost);
    EXPECT_FLOAT_EQ(held->Find(hatId)->BaseCost, cost) << "a caller keeps the set it was handed";
    std::cout << fmt::format("{} {} cost {} before the reload and {} after it, in a copy of the install's Root.wad\n", hatId, location->Path, cost, editedCost);

    uint64 const editedGeneration = items.GetGeneration();
    std::vector<std::string> errors;
    ASSERT_TRUE(sTypeRegistry.ClearSupplement(errors));
    ReloadOutcome const broken = sReloadMgr.Reload(ItemMgr::Target);
    for (std::string const& line : ReloadMgr::Describe(broken))
        std::cout << line << "\n";
    TypeDumpLoader::RawDump classes;
    std::string source;
    bool const restored = InstalledClasses::Read(classes, source, error) && sTypeRegistry.SetSupplement(std::move(classes), source, errors);
    sReloadMgr.Clear();
    ASSERT_TRUE(restored) << error;
    EXPECT_FALSE(broken.Ok);
    bool named = false;
    for (std::string const& line : broken.Errors)
        named = named || line.find("class hash 1064312042") != std::string::npos;
    EXPECT_TRUE(named) << "the reload names ReqMonsterMagicLevel's hash, which only the install's classes describe";
    EXPECT_EQ(items.GetGeneration(), editedGeneration);
    EXPECT_FLOAT_EQ(items.GetItems()->Find(hatId)->BaseCost, editedCost) << "the set serving before the failed reload keeps serving";
}
