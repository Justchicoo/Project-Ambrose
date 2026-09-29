/*
 * Project Ambrose by Imjustchico
 * Tests the supervisor's live settings over its own store: a change and its audit row land in one transaction and a batch whose audit row cannot be written leaves nothing behind, a Files setting set live is still set after the store is closed and opened again, a reset removes the value and keeps its history, history reads newest first in epoch seconds, and a closed store refuses plainly.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "PanelSettingStore.h"
#include "PanelStore.h"
#include "Settings.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
    SettingAuthor const Merle{ "Merle", 1, "panel" };

    class PanelSettingStoreTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _file = _directory.Path() / "panel.sqlite3";
            _configFile = _directory.Write("supervisor.conf", "Files.ListMaxEntries = 5000\n");
            Reopen();
        }

        void Reopen()
        {
            _store.Close();
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_file, Ambrose::FindSourceFolder(), warnings, error)) << error;
        }

        std::unique_ptr<ConfigMgr> Config()
        {
            auto config = std::make_unique<ConfigMgr>([](std::string const&) { return std::optional<std::string>(); });
            EXPECT_TRUE(config->LoadInitial(_configFile).Succeeded());
            return config;
        }

        void Start(Settings& settings, ConfigMgr& config)
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(settings.DeclareFor(SettingApps::Supervisor, errors)) << (errors.empty() ? std::string() : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(settings.Start(config, std::make_shared<PanelSettingStore>(_store, _mutex), warnings));
        }

        LogTestDirectory _directory;
        std::filesystem::path _file;
        std::filesystem::path _configFile;
        PanelStore _store;
        std::mutex _mutex;
    };
}

TEST_F(PanelSettingStoreTest, AChangeAndItsAuditRowLandInOneTransaction)
{
    PanelSettingStore store(_store, _mutex);
    SettingWrite write;
    write.Key = "Files.ReadMaxBytes";
    write.Persisted = "131072";
    write.OldValue = "4194304";
    write.NewValue = "131072";
    write.Author = Merle;
    write.Reason = "smaller reads";
    write.EpochSeconds = 1790000000;
    std::string error;
    ASSERT_TRUE(store.Write(write, error)) << error;

    std::map<std::string, std::string, std::less<>> values;
    ASSERT_TRUE(store.Load(values, error)) << error;
    EXPECT_EQ(values["Files.ReadMaxBytes"], "131072");
    std::vector<SettingAuditEntry> history;
    ASSERT_TRUE(store.History("Files.ReadMaxBytes", 10, history, error)) << error;
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].Who, "Merle");
    EXPECT_EQ(history[0].Reason, "smaller reads");
    EXPECT_EQ(history[0].EpochSeconds, 1790000000);

    ASSERT_TRUE(_store.Execute("DROP TABLE setting_audit", error)) << error;
    SettingWrite second = write;
    second.Key = "Files.ListMaxEntries";
    second.Persisted = "2000";
    std::vector<SettingWrite> const batch{ second };
    EXPECT_FALSE(store.WriteMany(std::span<SettingWrite const>(batch), error));
    EXPECT_FALSE(error.empty());
    values.clear();
    ASSERT_TRUE(store.Load(values, error)) << error;
    EXPECT_FALSE(values.contains("Files.ListMaxEntries")) << "a change whose audit row could not be written is not kept";
}

TEST_F(PanelSettingStoreTest, SupervisorFilesSettingsSurviveReopeningAndHistoryReadsNewestFirst)
{
    {
        std::unique_ptr<ConfigMgr> config = Config();
        Settings settings;
        Start(settings, *config);
        EXPECT_EQ(settings.Get<uint64>("Files.ListMaxEntries"), 5000u) << "the config value holds until a live change";
        ASSERT_TRUE(settings.Set("Files.ReadMaxBytes", "131072", Merle, "first").Ok());
        ASSERT_TRUE(settings.Set("Files.ReadMaxBytes", "262144", Merle, "second").Ok());
        ASSERT_TRUE(settings.Set("Files.MinFreePercent", "10", Merle, "more headroom").Ok());
        EXPECT_EQ(settings.Set("Files.MinFreePercent", "95", Merle, "too much").Result, SettingResult::OutOfBounds);
        EXPECT_EQ(settings.Set("Files.ReadMaxBytes", "1024", Merle, "too small").Result, SettingResult::OutOfBounds);
    }

    Reopen();
    std::unique_ptr<ConfigMgr> config = Config();
    Settings settings;
    Start(settings, *config);
    EXPECT_EQ(settings.Get<uint64>("Files.ReadMaxBytes"), 262144u);
    EXPECT_EQ(settings.Get<uint64>("Files.MinFreePercent"), 10u);
    EXPECT_EQ(settings.Get<uint64>("Files.MinFreeBytes"), 1073741824u) << "a setting nobody changed reads its declared default";

    std::vector<SettingAuditEntry> history;
    std::string error;
    ASSERT_TRUE(settings.History("Files.ReadMaxBytes", history, error)) << error;
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].Reason, "second");
    EXPECT_EQ(history[1].Reason, "first");

    ASSERT_TRUE(settings.Reset("Files.ReadMaxBytes", Merle, "back to the default").Ok());
    EXPECT_EQ(settings.Get<uint64>("Files.ReadMaxBytes"), 4194304u);
    history.clear();
    ASSERT_TRUE(settings.History("Files.ReadMaxBytes", history, error)) << error;
    EXPECT_EQ(history.size(), 3u) << "a reset is recorded like any change";
}

TEST_F(PanelSettingStoreTest, AClosedStoreRefusesPlainly)
{
    _store.Close();
    PanelSettingStore store(_store, _mutex);
    std::map<std::string, std::string, std::less<>> values;
    std::string error;
    EXPECT_FALSE(store.Load(values, error));
    EXPECT_NE(error.find("not open"), std::string::npos) << error;
    SettingWrite write;
    write.Key = "Files.ReadMaxBytes";
    EXPECT_FALSE(store.Write(write, error));
    std::vector<SettingAuditEntry> history;
    EXPECT_FALSE(store.History("Files.ReadMaxBytes", 5, history, error));
}
