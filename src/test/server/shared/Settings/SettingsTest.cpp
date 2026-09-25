/*
 * Project Ambrose by Imjustchico
 * Tests the live settings registry over an in-memory store and a config file the test writes: every declaration in the table passes its own checks and a contradictory one is refused; a value of the wrong type or out of bounds is refused naming the type or bound with nothing persisted, audited or announced; exactly one change is announced per successful set and none for a refused or unchanged one; a key an environment variable or command-line override sets is locked and the refusal names that layer; a live value outranks the config file for sSettings and ConfigMgr readers alike and a reset returns to the config value; a persisted value the bounds refuse falls back to the config value with a warning; a config reload announces only keys whose value changed; a store that fails changes nothing; a table key reads its declared default before an app declares it; and the settings command lists, gets, sets, resets and reads history.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "SettingDeclarations.h"
#include "Settings.h"
#include "SettingsCommand.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class MemoryStore : public SettingStore
    {
    public:
        bool Load(std::map<std::string, std::string, std::less<>>& values, std::string&) override
        {
            values = Values;
            return true;
        }

        bool Write(SettingWrite const& write, std::string& error) override
        {
            if (FailWrites)
            {
                error = "the test refuses every write";
                return false;
            }
            Writes.push_back(write);
            if (write.Persisted)
                Values[write.Key] = *write.Persisted;
            else
                Values.erase(write.Key);
            return true;
        }

        bool History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string&) override
        {
            for (auto write = Writes.rbegin(); write != Writes.rend() && entries.size() < limit; ++write)
                if (write->Key == key)
                    entries.push_back({ 0, write->Key, write->OldValue, write->NewValue, write->Author.Who, write->Author.AccountId, write->Author.Source, write->Reason, write->EpochSeconds });
            return true;
        }

        std::map<std::string, std::string, std::less<>> Values;
        std::vector<SettingWrite> Writes;
        bool FailWrites = false;
    };

    SettingAuthor const Merle{ "Merle", 7, "console" };

    class SettingsTest : public testing::Test
    {
    protected:
        std::unique_ptr<ConfigMgr> Config(std::string const& body, std::map<std::string, std::string> environment = {}, std::vector<std::pair<std::string, std::string>> overrides = {})
        {
            _file = _directory.Write("settings.conf", body);
            auto config = std::make_unique<ConfigMgr>([environment = std::move(environment)](std::string const& name) -> std::optional<std::string>
            {
                auto const found = environment.find(name);
                return found == environment.end() ? std::nullopt : std::optional<std::string>(found->second);
            });
            EXPECT_TRUE(config->LoadInitial(_file, {}, std::move(overrides)).Succeeded());
            return config;
        }

        void Open(Settings& settings, ConfigMgr& config, std::shared_ptr<SettingStore> store, std::vector<std::string>* warnings = nullptr)
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(settings.DeclareFor(SettingApps::Game, errors)) << errors.front();
            std::vector<std::string> collected;
            ASSERT_TRUE(settings.Start(config, std::move(store), collected));
            if (warnings)
                *warnings = std::move(collected);
        }

        static std::vector<SettingChange> Dispatch(Settings& settings)
        {
            std::vector<SettingChange> seen;
            uint64 const token = settings.Subscribe([&seen](SettingChange const& change) { seen.push_back(change); });
            settings.DispatchChanges();
            settings.Unsubscribe(token);
            return seen;
        }

        LogTestDirectory _directory;
        std::filesystem::path _file;
    };
}

TEST_F(SettingsTest, EveryDeclarationInTheTablePassesItsOwnChecksOnce)
{
    Settings settings;
    std::vector<SettingDeclaration> const& table = SettingDeclarations::All();
    ASSERT_FALSE(table.empty());
    for (SettingDeclaration const& declaration : table)
    {
        std::string error;
        EXPECT_TRUE(settings.Declare(declaration, error)) << declaration.Key << ": " << error;
        EXPECT_NE(declaration.Apps, 0) << declaration.Key << " is read by no app";
        EXPECT_EQ(SettingDeclarations::Find(declaration.Key), &declaration);
    }
    EXPECT_TRUE(std::adjacent_find(table.begin(), table.end(), [](SettingDeclaration const& left, SettingDeclaration const& right) { return left.Key >= right.Key; }) == table.end())
        << "the table is sorted and names each key once";
    EXPECT_EQ(SettingDeclarations::Find("No.Such.Setting"), nullptr);
}

TEST_F(SettingsTest, ADeclarationThatContradictsItselfIsRefused)
{
    Settings settings;
    std::string error;
    SettingDeclaration base{ "Test.Value", SettingType::Unsigned, "5", "1", "10", "s", "Test", "a value for the test", SettingApply::Live, {}, SettingApps::Game };
    ASSERT_TRUE(settings.Declare(base, error)) << error;
    EXPECT_TRUE(settings.Declare(base, error)) << "the same declaration twice is taken once";

    SettingDeclaration changed = base;
    changed.Default = "6";
    EXPECT_FALSE(settings.Declare(changed, error));
    EXPECT_NE(error.find("declared twice, differently"), std::string::npos) << error;

    SettingDeclaration outside = base;
    outside.Key = "Test.Outside";
    outside.Default = "11";
    EXPECT_FALSE(settings.Declare(outside, error));
    EXPECT_NE(error.find("a default its own bounds refuse"), std::string::npos) << error;

    SettingDeclaration upsideDown = base;
    upsideDown.Key = "Test.UpsideDown";
    upsideDown.Min = "10";
    upsideDown.Max = "1";
    EXPECT_FALSE(settings.Declare(upsideDown, error));

    SettingDeclaration restart = base;
    restart.Key = "Test.Restart";
    restart.Apply = SettingApply::Restart;
    EXPECT_FALSE(settings.Declare(restart, error));
    EXPECT_NE(error.find("must say why"), std::string::npos) << error;

    SettingDeclaration flag{ "Test.Flag", SettingType::Bool, "true", "0", {}, {}, "Test", "a flag", SettingApply::Live, {}, SettingApps::Game };
    EXPECT_FALSE(settings.Declare(flag, error)) << "a flag has no bounds";

    SettingDeclaration badKey = base;
    badKey.Key = "1st value";
    EXPECT_FALSE(settings.Declare(badKey, error));
}

TEST_F(SettingsTest, AWrongTypeOrOutOfBoundsValueIsRefusedNamingItAndNothingIsPersistedAuditedOrAnnounced)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);

    SettingOutcome const wrongType = settings.Set("World.UpdateInterval", "fast", Merle, "");
    EXPECT_EQ(wrongType.Result, SettingResult::WrongType);
    EXPECT_NE(wrongType.Message.find("a whole number of zero or more from 1 to 10000 ms"), std::string::npos) << wrongType.Message;

    SettingOutcome const outside = settings.Set("World.UpdateInterval", "20000", Merle, "");
    EXPECT_EQ(outside.Result, SettingResult::OutOfBounds);
    EXPECT_NE(outside.Message.find("must be from 1 to 10000 ms; 20000 is outside that"), std::string::npos) << outside.Message;

    SettingOutcome const longText = settings.Set("GM.CommandPrefix", "!!!!!!!!!", Merle, "");
    EXPECT_EQ(longText.Result, SettingResult::OutOfBounds) << longText.Message;

    EXPECT_EQ(settings.Set("No.Such.Setting", "1", Merle, "").Result, SettingResult::UnknownKey);
    EXPECT_TRUE(store->Writes.empty()) << "a refused value is neither persisted nor audited";
    EXPECT_TRUE(Dispatch(settings).empty()) << "and nothing is announced";
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 50u);
}

TEST_F(SettingsTest, ExactlyOneChangeIsAnnouncedPerSuccessfulSetAndNoneForARefusedOrUnchangedOne)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);

    ASSERT_TRUE(settings.Set("World.UpdateInterval", "100", Merle, "faster ticks").Ok());
    EXPECT_EQ(settings.GetPendingChangeCount(), 1u);
    std::vector<SettingChange> const changes = Dispatch(settings);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes.front().Key, "World.UpdateInterval");
    EXPECT_EQ(changes.front().OldValue, "50");
    EXPECT_EQ(changes.front().NewValue, "100");

    EXPECT_EQ(settings.Set("World.UpdateInterval", "100", Merle, "").Result, SettingResult::Unchanged);
    EXPECT_EQ(settings.Set("World.UpdateInterval", "0", Merle, "").Result, SettingResult::OutOfBounds);
    EXPECT_TRUE(Dispatch(settings).empty()) << "an unchanged or refused set announces nothing";
    EXPECT_EQ(store->Writes.size(), 1u) << "and writes nothing";
}

TEST_F(SettingsTest, AKeyAnEnvironmentVariableOrOverrideSetsIsLockedAndTheRefusalNamesThatLayer)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\nZone.UnloadDelay = 60\n", { { "AMBROSE_WORLD_UPDATE_INTERVAL", "75" } },
        { { "Zone.UnloadDelay", "90" } });
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 75u);
    EXPECT_EQ(settings.Get<uint32>("Zone.UnloadDelay"), 90u);

    SettingOutcome const environment = settings.Set("World.UpdateInterval", "100", Merle, "");
    EXPECT_EQ(environment.Result, SettingResult::Locked);
    EXPECT_NE(environment.Message.find("the environment variable AMBROSE_WORLD_UPDATE_INTERVAL"), std::string::npos) << environment.Message;
    SettingOutcome const override = settings.Set("Zone.UnloadDelay", "120", Merle, "");
    EXPECT_EQ(override.Result, SettingResult::Locked);
    EXPECT_NE(override.Message.find("a command-line override"), std::string::npos) << override.Message;
    EXPECT_EQ(settings.Reset("World.UpdateInterval", Merle, "").Result, SettingResult::Locked);
    EXPECT_TRUE(store->Writes.empty());
    std::optional<SettingView> const view = settings.Describe("World.UpdateInterval");
    ASSERT_TRUE(view);
    EXPECT_EQ(view->Layer, SettingLayer::Environment);
}

TEST_F(SettingsTest, ALiveValueOutranksTheConfigFileForEveryReaderAndAResetReturnsToTheConfigValue)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 70\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    ASSERT_TRUE(settings.Set("World.UpdateInterval", "100", Merle, "").Ok());
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 100u);
    EXPECT_EQ(config->GetOption<uint32>("World.UpdateInterval", 0, true), 100u) << "a reader of the config sees the live value too";
    std::optional<SettingView> const live = settings.Describe("World.UpdateInterval");
    ASSERT_TRUE(live);
    EXPECT_EQ(live->Layer, SettingLayer::Live);
    EXPECT_EQ(live->Persisted, "100");

    SettingOutcome const reset = settings.Reset("World.UpdateInterval", Merle, "back to normal");
    ASSERT_TRUE(reset.Ok()) << reset.Message;
    EXPECT_NE(reset.Message.find("is back to 70 from settings.conf line 1"), std::string::npos) << reset.Message;
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 70u);
    EXPECT_EQ(config->GetOption<uint32>("World.UpdateInterval", 0, true), 70u);
    EXPECT_EQ(settings.Reset("World.UpdateInterval", Merle, "").Result, SettingResult::Unchanged) << "a key with no live value has nothing to reset";
    ASSERT_EQ(store->Writes.size(), 2u);
    EXPECT_FALSE(store->Writes.back().Persisted) << "a reset removes the persisted row";
    EXPECT_EQ(store->Writes.back().OldValue, "100");
    EXPECT_EQ(store->Writes.back().NewValue, "70");
}

TEST_F(SettingsTest, APersistedValueTheBoundsRefuseFallsBackToTheConfigValueWithAWarning)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 70\n");
    auto const store = std::make_shared<MemoryStore>();
    store->Values["World.UpdateInterval"] = "99999";
    store->Values["Retired.Setting"] = "1";
    Settings settings;
    std::vector<std::string> warnings;
    Open(settings, *config, store, &warnings);
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 70u) << "a value a newer binary refuses does not stop the app; the config value serves";
    EXPECT_EQ(config->GetOption<uint32>("World.UpdateInterval", 0, true), 70u) << "and it never reaches the config's live layer";
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(), [](std::string const& warning) { return warning.find("from the settings table is refused") != std::string::npos; }));
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(), [](std::string const& warning) { return warning.find("Retired.Setting, which no app declares") != std::string::npos; }));
}

TEST_F(SettingsTest, AConfigReloadAnnouncesOnlyTheKeysWhoseValueChanged)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 70\nZone.UnloadDelay = 60\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    ASSERT_TRUE(settings.Set("Zone.UnloadDelay", "30", Merle, "").Ok());
    Dispatch(settings);

    _directory.Write("settings.conf", "World.UpdateInterval = 80\nZone.UnloadDelay = 90\n");
    ASSERT_TRUE(config->Reload().Succeeded());
    settings.Resolve();
    std::vector<SettingChange> const changes = Dispatch(settings);
    ASSERT_EQ(changes.size(), 1u) << "a key whose live value hides the edited file line does not change";
    EXPECT_EQ(changes.front().Key, "World.UpdateInterval");
    EXPECT_EQ(changes.front().NewValue, "80");
    EXPECT_EQ(settings.Get<uint32>("Zone.UnloadDelay"), 30u);
}

TEST_F(SettingsTest, AStoreThatFailsChangesNothing)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 70\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    store->FailWrites = true;
    SettingOutcome const outcome = settings.Set("World.UpdateInterval", "100", Merle, "");
    EXPECT_EQ(outcome.Result, SettingResult::StoreFailed);
    EXPECT_NE(outcome.Message.find("the test refuses every write"), std::string::npos) << outcome.Message;
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 70u);
    EXPECT_TRUE(Dispatch(settings).empty());

    Settings unopened;
    std::vector<std::string> errors;
    ASSERT_TRUE(unopened.DeclareFor(SettingApps::Game, errors));
    EXPECT_EQ(unopened.Set("World.UpdateInterval", "100", Merle, "").Result, SettingResult::NotStarted);
}

TEST_F(SettingsTest, ATableKeyReadsItsDeclaredDefaultBeforeAnyAppDeclaresIt)
{
    Settings settings;
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 50u);
    EXPECT_EQ(settings.Get<std::string>("Realm.Name"), "Ambrose");
    EXPECT_DOUBLE_EQ(settings.Get<double>("Rate.XP.Quest"), 1.0);
    EXPECT_EQ(settings.Get<uint32>("No.Such.Setting"), 0u) << "a key nothing declares reads empty and is reported";
}

TEST_F(SettingsTest, TheSettingsCommandListsGetsSetsResetsAndReadsHistory)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 70\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    std::vector<std::string> lines;
    auto const run = [&](std::vector<std::string> arguments)
    {
        lines.clear();
        return SettingsCommand::Run(settings, arguments, Merle, [&lines](std::string_view line) { lines.emplace_back(line); });
    };
    auto const said = [&](std::string_view text) { return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; }); };

    ASSERT_TRUE(run({ "list", "world" }));
    EXPECT_TRUE(said("World:"));
    EXPECT_TRUE(said("World.UpdateInterval = 70 ms (config)"));
    EXPECT_FALSE(said("Zone.UnloadDelay"));

    ASSERT_TRUE(run({ "get", "World.UpdateInterval" }));
    EXPECT_TRUE(said("from settings.conf line 1"));
    EXPECT_TRUE(said("unsigned from 1 to 10000 ms, default 50 ms, applies live"));

    ASSERT_TRUE(run({ "set", "World.UpdateInterval", "100", "faster", "ticks" }));
    EXPECT_TRUE(said("World.UpdateInterval is now 100"));
    ASSERT_TRUE(run({ "reset", "World.UpdateInterval" }));
    EXPECT_TRUE(said("is back to 70"));
    ASSERT_TRUE(run({ "history", "World.UpdateInterval" }));
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_TRUE(said("100 to 70 by Merle from console"));
    EXPECT_TRUE(said("70 to 100 by Merle from console: faster ticks"));

    EXPECT_FALSE(run({ "set", "World.UpdateInterval" })) << "a set with no value is a usage error";
    EXPECT_FALSE(run({ "fly" }));
    ASSERT_TRUE(run({ "set", "World.UpdateInterval", "fast" })) << "a refusal is the command used correctly";
    EXPECT_TRUE(said("is not one"));
}
