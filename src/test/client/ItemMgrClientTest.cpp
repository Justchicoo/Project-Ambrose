/*
 * Project Ambrose by Imjustchico
 * Reads every item template of the user's own install through the item manager, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it, with the classes the install holds beside the dump as the game server reads them: every template under ObjectData/ decodes and every item among them loads with no requirement or effect of a class neither describes, as many WizItemTemplates as recorded for the installed revision, r806919's 76679, printed with the other item classes, the memory they take and how long they took; and the hat 1652259 is an item under the display key Items_00028316.
 */

#include "Environment.h"
#include "InstalledClasses.h"
#include "InstalledRevision.h"
#include "ItemMgr.h"
#include "LogConfig.h"
#include "ObjectTemplateMgr.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
