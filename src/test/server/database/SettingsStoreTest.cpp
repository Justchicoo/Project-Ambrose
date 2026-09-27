/*
 * Project Ambrose by Imjustchico
 * Checks the settings store against real characters and login databases when AMBROSE_TEST_DB is set: a value set live survives a restart of the registry, a reset returns the key to its config value for later starts as well, every change writes exactly one setting_audit row with the value before and after, who made it, where it came in and why, newest first, while a refused set writes none, a batch the database refuses one row of leaves every row unwritten, a batch that lands persists each entry with its own audit row, and a secret longer than the old column limit is persisted whole while its audit rows hold only the mask.
 */

#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "DatabaseSettingStore.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "MySQLConnection.h"
#include "Settings.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    SettingAuthor const Merle{ "Merle", 7, "console" };

    class SettingsStoreTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            uint32 const tag = std::random_device()();
            _characters = *info;
            _characters.Database = fmt::format("ambrose_settings_chars_{:08x}", tag);
            _login = *info;
            _login.Database = fmt::format("ambrose_settings_login_{:08x}", tag);
            ASSERT_TRUE(DBUpdater::Run(_characters, "characters", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_login, "login", UpdaterSettings{}));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_characters.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_login.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            _open = true;
            _config = std::make_unique<ConfigMgr>();
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("settings.conf", "World.UpdateInterval = 70\nLogin.KeyTTL = 60\n")).Succeeded());
        }

        void TearDown() override
        {
            if (_open)
            {
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            for (MySQLConnectionInfo const* database : { &_characters, &_login })
            {
                if (database->Database.empty())
                    continue;
                MySQLConnectionInfo server = *database;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(database->Database)));
            }
        }

        void Open(Settings& settings, uint8 app, std::shared_ptr<SettingStore> store)
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(settings.DeclareFor(app, errors)) << errors.front();
            std::vector<std::string> warnings;
            ASSERT_TRUE(settings.Start(*_config, std::move(store), warnings)) << (warnings.empty() ? std::string() : warnings.front());
        }

        LogTestDirectory _directory;
        MySQLConnectionInfo _characters;
        MySQLConnectionInfo _login;
        std::unique_ptr<ConfigMgr> _config;
        bool _open = false;
    };
}

TEST_F(SettingsStoreTest, ASetValueSurvivesARestartAndAResetReturnsToTheConfigValue)
{
    {
        Settings first;
        Open(first, SettingApps::Game, SettingStores::ForCharacters());
        SettingOutcome const set = first.Set("World.UpdateInterval", "100", Merle, "faster ticks");
        ASSERT_TRUE(set.Ok()) << set.Message;
    }
    {
        Settings second;
        Open(second, SettingApps::Game, SettingStores::ForCharacters());
        EXPECT_EQ(second.Get<uint32>("World.UpdateInterval"), 100u) << "the live value outlives the registry that set it";
        SettingOutcome const reset = second.Reset("World.UpdateInterval", Merle, "back to the file");
        ASSERT_TRUE(reset.Ok()) << reset.Message;
        EXPECT_EQ(second.Get<uint32>("World.UpdateInterval"), 70u);
    }
    Settings third;
    Open(third, SettingApps::Game, SettingStores::ForCharacters());
    EXPECT_EQ(third.Get<uint32>("World.UpdateInterval"), 70u) << "a reset key starts from its config value again";
}

TEST_F(SettingsStoreTest, EveryChangeWritesOneAuditRowWithTheValuesWhoWhereAndWhy)
{
    Settings settings;
    Open(settings, SettingApps::Game, SettingStores::ForCharacters());
    ASSERT_TRUE(settings.Set("World.UpdateInterval", "100", Merle, "faster ticks").Ok());
    EXPECT_EQ(settings.Set("World.UpdateInterval", "0", Merle, "too fast").Result, SettingResult::OutOfBounds);
    ASSERT_TRUE(settings.Reset("World.UpdateInterval", SettingAuthor{ "Ambrose", 0, "admin api" }, "back to the file").Ok());

    std::vector<SettingAuditEntry> entries;
    std::string error;
    ASSERT_TRUE(settings.History("World.UpdateInterval", entries, error)) << error;
    ASSERT_EQ(entries.size(), 2u) << "one row per change, and none for the refused set";
    EXPECT_EQ(entries[0].OldValue, "100");
    EXPECT_EQ(entries[0].NewValue, "70");
    EXPECT_EQ(entries[0].Who, "Ambrose");
    EXPECT_EQ(entries[0].Source, "admin api");
    EXPECT_EQ(entries[0].Reason, "back to the file");
    EXPECT_EQ(entries[1].OldValue, "70");
    EXPECT_EQ(entries[1].NewValue, "100");
    EXPECT_EQ(entries[1].Who, "Merle");
    EXPECT_EQ(entries[1].AccountId, 7u);
    EXPECT_EQ(entries[1].Source, "console");
    EXPECT_EQ(entries[1].Reason, "faster ticks");
    EXPECT_GT(entries[1].EpochSeconds, 0);
    EXPECT_GT(entries[0].Id, entries[1].Id);
}

TEST_F(SettingsStoreTest, TheLoginDatabaseKeepsTheLoginServersSettings)
{
    {
        Settings first;
        Open(first, SettingApps::Login, SettingStores::ForLogin());
        ASSERT_TRUE(first.Set("Login.KeyTTL", "120", Merle, "slower clients").Ok());
    }
    Settings second;
    Open(second, SettingApps::Login, SettingStores::ForLogin());
    EXPECT_EQ(second.Get<uint32>("Login.KeyTTL"), 120u);
    std::vector<SettingAuditEntry> entries;
    std::string error;
    ASSERT_TRUE(second.History("Login.KeyTTL", entries, error)) << error;
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].Reason, "slower clients");
}

TEST_F(SettingsStoreTest, ABatchTheDatabaseRefusesOneRowOfLeavesEveryRowUnwritten)
{
    std::shared_ptr<SettingStore> const store = SettingStores::ForCharacters();
    std::vector<SettingWrite> writes(2);
    writes[0].Key = "World.UpdateInterval";
    writes[0].Persisted = "100";
    writes[0].OldValue = "70";
    writes[0].NewValue = "100";
    writes[0].Author = Merle;
    writes[0].Reason = "faster ticks";
    writes[0].EpochSeconds = 1;
    writes[1] = writes[0];
    writes[1].Key = std::string(200, 'K');
    std::string error;
    EXPECT_FALSE(store->WriteMany(writes, error)) << "the key column holds 128 characters, so the second row is refused";
    std::map<std::string, std::string, std::less<>> values;
    ASSERT_TRUE(store->Load(values, error)) << error;
    EXPECT_TRUE(values.empty()) << "the first row went in the same transaction as the refused one";
    std::vector<SettingAuditEntry> entries;
    ASSERT_TRUE(store->History("World.UpdateInterval", 20, entries, error)) << error;
    EXPECT_TRUE(entries.empty());
}

TEST_F(SettingsStoreTest, ABatchPersistsEveryEntryWithAnAuditRowEach)
{
    {
        Settings first;
        Open(first, SettingApps::Game, SettingStores::ForCharacters());
        std::vector<SettingEntry> const entries{ { "World.UpdateInterval", "100" }, { "World.Heartbeat", "30" } };
        SettingBatchOutcome const outcome = first.SetMany(entries, Merle, "tuning");
        ASSERT_TRUE(outcome.Ok()) << outcome.Message;
        ASSERT_EQ(outcome.Changes.size(), 2u);
    }
    Settings second;
    Open(second, SettingApps::Game, SettingStores::ForCharacters());
    EXPECT_EQ(second.Get<uint32>("World.UpdateInterval"), 100u);
    EXPECT_EQ(second.Get<uint32>("World.Heartbeat"), 30u);
    for (char const* key : { "World.UpdateInterval", "World.Heartbeat" })
    {
        std::vector<SettingAuditEntry> entries;
        std::string error;
        ASSERT_TRUE(second.History(key, entries, error)) << error;
        ASSERT_EQ(entries.size(), 1u) << key;
        EXPECT_EQ(entries[0].Reason, "tuning");
    }
}

TEST_F(SettingsStoreTest, ASecretLongerThanTheOldColumnIsKeptWholeAndAuditedMasked)
{
    std::string ring;
    for (int id = 1; id <= 20; ++id)
        ring += fmt::format("{}{}:{}", ring.empty() ? "" : ",", id, std::string(64, "0123456789abcdef"[id % 16]));
    ASSERT_GT(ring.size(), 1024u);
    {
        Settings first;
        Open(first, SettingApps::Login, SettingStores::ForLogin());
        SettingOutcome const set = first.Set("Account.VerifierKeys", ring, Merle, "a full ring");
        ASSERT_TRUE(set.Ok()) << set.Message;
        EXPECT_EQ(set.Message.find(std::string(64, '1')), std::string::npos) << set.Message;
    }
    Settings second;
    Open(second, SettingApps::Login, SettingStores::ForLogin());
    EXPECT_EQ(second.Get<std::string>("Account.VerifierKeys"), ring);
    std::vector<SettingAuditEntry> entries;
    std::string error;
    ASSERT_TRUE(second.History("Account.VerifierKeys", entries, error)) << error;
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].OldValue, "");
    EXPECT_TRUE(entries[0].NewValue.starts_with("1:***,2:***,3:***")) << entries[0].NewValue;
    EXPECT_EQ(entries[0].NewValue.find("aaaa"), std::string::npos) << "the key material never reaches the audit table";
}
