/*
 * Project Ambrose by Imjustchico
 * Tests the live settings registry over an in-memory store and a config file the test writes: every declaration in the table passes its own checks and a contradictory one is refused; a value of the wrong type or out of bounds is refused naming the type or bound with nothing persisted, audited or announced; exactly one change is announced per successful set and none for a refused or unchanged one; a key an environment variable or command-line override sets is locked and the refusal names that layer; a live value outranks the config file for sSettings and ConfigMgr readers alike and a reset returns to the config value; a persisted value the bounds refuse falls back to the config value with a warning; a config reload announces only keys whose value changed; a store that fails changes nothing; a table key reads its declared default before an app declares it; the settings command lists, gets, sets, resets and reads history; validating a batch names every bad entry, a batch with one bad entry applies none and one whose store fails changes nothing, while a good batch is one commit announced once; a check sees the values proposed beside its own; a secret is masked in every message, log line, audit row and command answer; and a watcher hears a change on the writing thread before any tick.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
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
#include <thread>
#include <vector>

namespace
{
    using MemoryStore = MemorySettingStore;

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

        void Open(Settings& settings, ConfigMgr& config, std::shared_ptr<SettingStore> store, std::vector<std::string>* warnings = nullptr, uint8 app = SettingApps::Game)
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(settings.DeclareFor(app, errors)) << errors.front();
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

TEST_F(SettingsTest, ValidatingABatchNamesEveryBadEntry)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n", { { "AMBROSE_WORLD_HEARTBEAT", "30" } });
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);

    std::vector<SettingEntry> const entries{ { "World.UpdateInterval", "0" }, { "World.Heartbeat", "10" }, { "Rate.XP.Quest", "fast" }, { "No.Such.Setting", "1" },
        { "Zone.UnloadDelay", "30" }, { "Zone.UnloadDelay", "40" }, { "Rate.XP.Kill", "2" } };
    std::vector<SettingProblem> const problems = settings.Validate(entries);
    ASSERT_EQ(problems.size(), 5u);
    EXPECT_EQ(problems[0].Key, "World.UpdateInterval");
    EXPECT_EQ(problems[0].Result, SettingResult::OutOfBounds);
    EXPECT_EQ(problems[1].Key, "World.Heartbeat");
    EXPECT_EQ(problems[1].Result, SettingResult::Locked);
    EXPECT_NE(problems[1].Message.find("AMBROSE_WORLD_HEARTBEAT"), std::string::npos) << problems[1].Message;
    EXPECT_EQ(problems[2].Result, SettingResult::WrongType);
    EXPECT_EQ(problems[3].Result, SettingResult::UnknownKey);
    EXPECT_EQ(problems[4].Key, "Zone.UnloadDelay");
    EXPECT_EQ(problems[4].Result, SettingResult::Duplicate);
    EXPECT_TRUE(store->Writes.empty()) << "validating writes nothing";
}

TEST_F(SettingsTest, ABatchWithOneBadEntryAppliesNoneAndNamesEveryBadOne)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n", { { "AMBROSE_WORLD_HEARTBEAT", "30" } });
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);

    std::vector<SettingEntry> const entries{ { "World.UpdateInterval", "100" }, { "Rate.XP.Quest", "500" }, { "World.Heartbeat", "10" } };
    SettingBatchOutcome const outcome = settings.SetMany(entries, Merle, "tuning");
    EXPECT_FALSE(outcome.Ok());
    ASSERT_EQ(outcome.Problems.size(), 2u) << outcome.Message;
    EXPECT_EQ(outcome.Problems[0].Key, "Rate.XP.Quest");
    EXPECT_EQ(outcome.Problems[0].Result, SettingResult::OutOfBounds);
    EXPECT_EQ(outcome.Problems[1].Key, "World.Heartbeat");
    EXPECT_EQ(outcome.Problems[1].Result, SettingResult::Locked);
    EXPECT_TRUE(outcome.Changes.empty());
    EXPECT_TRUE(store->Writes.empty()) << "the good entry is not written either";
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 50u);
    EXPECT_EQ(settings.GetPendingChangeCount(), 0u);
}

TEST_F(SettingsTest, ABatchWhoseStoreFailsChangesNothingAndAGoodBatchIsOneCommitAnnouncedOnce)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    std::vector<SettingEntry> const entries{ { "World.UpdateInterval", "100" }, { "World.Heartbeat", "30" }, { "Rate.XP.Quest", "1" } };

    store->FailWrites = true;
    SettingBatchOutcome const failed = settings.SetMany(entries, Merle, "tuning");
    EXPECT_EQ(failed.Result, SettingResult::StoreFailed);
    EXPECT_NE(failed.Message.find("the test refuses every write"), std::string::npos) << failed.Message;
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 50u);
    EXPECT_EQ(settings.Get<uint32>("World.Heartbeat"), 60u);
    EXPECT_TRUE(Dispatch(settings).empty());

    store->FailWrites = false;
    SettingBatchOutcome const landed = settings.SetMany(entries, Merle, "tuning");
    ASSERT_TRUE(landed.Ok()) << landed.Message;
    EXPECT_EQ(store->Commits, 1u);
    EXPECT_EQ(store->Writes.size(), 2u);
    ASSERT_EQ(landed.Unchanged.size(), 1u);
    EXPECT_EQ(landed.Unchanged.front(), "Rate.XP.Quest") << "a value already held is reported, not written";
    EXPECT_EQ(settings.Get<uint32>("World.UpdateInterval"), 100u);
    EXPECT_EQ(settings.Get<uint32>("World.Heartbeat"), 30u);
    std::vector<SettingChange> const changes = Dispatch(settings);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].Author.Who, "Merle");
    EXPECT_EQ(changes[0].Reason, "tuning");
    EXPECT_EQ(config->GetOption<uint32>("World.Heartbeat", 0), 30u) << "ConfigMgr readers see the batch too";
}

TEST_F(SettingsTest, ACheckSeesTheValuesProposedBesideItsOwn)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    Settings::Check const heartbeatOutlastsTick = [](std::string_view, Settings::ProposedValue const& proposed) -> std::optional<std::string>
    {
        uint64 const tick = std::stoull(proposed("World.UpdateInterval"));
        uint64 const heartbeat = std::stoull(proposed("World.Heartbeat"));
        if (heartbeat != 0 && heartbeat * 1000 < tick)
            return std::string("World.Heartbeat must be longer than one tick");
        return std::nullopt;
    };
    ASSERT_TRUE(settings.AddCheck("World.UpdateInterval", heartbeatOutlastsTick));
    ASSERT_TRUE(settings.AddCheck("World.Heartbeat", heartbeatOutlastsTick));
    EXPECT_FALSE(settings.AddCheck("No.Such.Setting", heartbeatOutlastsTick));

    SettingOutcome const alone = settings.Set("World.Heartbeat", "1", Merle, "");
    ASSERT_TRUE(alone.Ok()) << alone.Message;
    std::vector<SettingEntry> const clash{ { "World.UpdateInterval", "5000" }, { "World.Heartbeat", "2" } };
    SettingBatchOutcome const refused = settings.SetMany(clash, Merle, "");
    ASSERT_EQ(refused.Problems.size(), 2u) << "each check reads the other entry's proposed value, not the one in force";
    EXPECT_EQ(refused.Problems[0].Result, SettingResult::Invalid);
    std::vector<SettingEntry> const fits{ { "World.UpdateInterval", "5000" }, { "World.Heartbeat", "10" } };
    EXPECT_TRUE(settings.SetMany(fits, Merle, "").Ok());
    EXPECT_EQ(settings.Set("World.UpdateInterval", "20000", Merle, "").Result, SettingResult::OutOfBounds) << "the declared bounds still come first";
}

TEST_F(SettingsTest, ASecretIsMaskedInEveryMessageLogLineAuditRowAndCommandAnswer)
{
    std::string const first = "1:" + std::string(64, 'a');
    std::string const both = first + ",2:" + std::string(64, 'b');
    std::unique_ptr<ConfigMgr> const config = Config("Account.VerifierKeys = " + first + "\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store, nullptr, SettingApps::Login);
    EXPECT_TRUE(settings.IsSecret("Account.VerifierKeys"));
    EXPECT_TRUE(settings.IsSecret("LoginDatabaseInfo"));
    EXPECT_FALSE(settings.IsSecret("Account.VerifierActiveKey"));
    EXPECT_EQ(settings.Get<std::string>("Account.VerifierKeys"), first) << "the registry itself holds the real value";

    auto const clean = [](std::string const& text) { return text.find("aaaa") == std::string::npos && text.find("bbbb") == std::string::npos; };
    SettingOutcome const set = settings.Set("Account.VerifierKeys", both, Merle, "a second key");
    ASSERT_TRUE(set.Ok()) << set.Message;
    EXPECT_TRUE(clean(set.Message)) << set.Message;
    EXPECT_NE(set.Message.find("1:***,2:***"), std::string::npos) << set.Message;
    EXPECT_TRUE(clean(settings.Set("Account.VerifierKeys", both, Merle, "").Message));
    SettingOutcome const wrong = settings.Set("Account.VerifierKeys", std::string(70000, 'c'), Merle, "");
    EXPECT_EQ(wrong.Message.find("cccc"), std::string::npos) << wrong.Message;
    ASSERT_EQ(store->Writes.size(), 1u);
    EXPECT_EQ(store->Writes[0].Persisted, both) << "the settings table keeps the value it must apply";
    EXPECT_EQ(store->Writes[0].OldValue, "1:***");
    EXPECT_EQ(store->Writes[0].NewValue, "1:***,2:***") << "the audit row keeps only the mask";

    std::vector<std::string> lines;
    auto const run = [&](std::vector<std::string> arguments)
    {
        lines.clear();
        return SettingsCommand::Run(settings, arguments, Merle, [&lines](std::string_view line) { lines.emplace_back(line); });
    };
    for (std::vector<std::string> const& command : { std::vector<std::string>{ "list" }, { "get", "Account.VerifierKeys" }, { "history", "Account.VerifierKeys" },
             { "reset", "Account.VerifierKeys" } })
    {
        ASSERT_TRUE(run(command));
        for (std::string const& line : lines)
            EXPECT_TRUE(clean(line)) << line;
    }
}

TEST_F(SettingsTest, AWatcherHearsAChangeOnTheWritingThreadBeforeAnyTick)
{
    std::unique_ptr<ConfigMgr> const config = Config("World.UpdateInterval = 50\n");
    auto const store = std::make_shared<MemoryStore>();
    Settings settings;
    Open(settings, *config, store);
    std::vector<SettingChange> heard;
    std::thread::id heardOn;
    uint64 const token = settings.Watch([&](SettingChange const& change)
    {
        heard.push_back(change);
        heardOn = std::this_thread::get_id();
    });

    ASSERT_TRUE(settings.Set("World.UpdateInterval", "100", Merle, "faster ticks").Ok());
    ASSERT_EQ(heard.size(), 1u) << "heard before DispatchChanges ran";
    EXPECT_EQ(heardOn, std::this_thread::get_id());
    EXPECT_EQ(heard[0].OldValue, "50");
    EXPECT_EQ(heard[0].NewValue, "100");
    EXPECT_EQ(heard[0].Author.Who, "Merle");
    EXPECT_EQ(heard[0].Reason, "faster ticks");
    EXPECT_GT(heard[0].EpochSeconds, 0);
    EXPECT_EQ(settings.GetPendingChangeCount(), 1u) << "the tick still gets its own copy";

    ASSERT_TRUE(settings.Reset("World.UpdateInterval", Merle, "").Ok());
    EXPECT_EQ(heard.size(), 2u);
    settings.Unwatch(token);
    ASSERT_TRUE(settings.Set("World.UpdateInterval", "100", Merle, "").Ok());
    EXPECT_EQ(heard.size(), 2u) << "an unwatched handler hears nothing more";
}
