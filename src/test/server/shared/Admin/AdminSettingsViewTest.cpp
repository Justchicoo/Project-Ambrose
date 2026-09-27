/*
 * Project Ambrose by Imjustchico
 * Tests the settings API through the admin router over an in-memory store: an out-of-bounds PUT answers 422 and changes nothing, a PUT missing its reason with a bad value names both, a PUT to a key an environment variable or a command-line override sets answers 409 naming the layer and the variable, a batch with one bad entry answers 422 naming every bad one and applies none while a good batch lands in one commit, Account.VerifierKeys reads masked unless the caller asks with the right and every reveal is recorded, its history is masked even to a caller who may reveal, a restricted key needs its own right and a supervisor's forwarded grants narrow a token caller, a relayed change is attributed to the user the supervisor names, bodies that are not a change are refused field by field, a reset returns a key to its config value with an audit row of its own, a reset a registered check refuses answers 422 and changes nothing, a dry run says what a batch would change or refuse and changes nothing, and a reveal naming one key shows and records only that one.
 */

#include "AdminAuth.h"
#include "AdminConfigView.h"
#include "AdminRouter.h"
#include "AdminSettingsView.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    class AdminSettingsViewTest : public testing::Test
    {
    protected:
        void Open(std::string const& body, uint8 app = SettingApps::Game, std::map<std::string, std::string> environment = {},
            std::vector<std::pair<std::string, std::string>> overrides = {})
        {
            _config = std::make_unique<ConfigMgr>([environment = std::move(environment)](std::string const& name) -> std::optional<std::string>
            {
                auto const found = environment.find(name);
                return found == environment.end() ? std::nullopt : std::optional<std::string>(found->second);
            });
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("app.conf", body), {}, std::move(overrides)).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(_settings.DeclareFor(app, errors)) << errors.front();
            std::vector<std::string> warnings;
            ASSERT_TRUE(_settings.Start(*_config, _store, warnings));
            _auth.SetToken(Token);
            AdminConfigView::Register(_routes, *_config, {}, &_settings, [this](AdminRequest const&, std::vector<std::string> const& keys) { _reveals.push_back(keys); });
            AdminSettingsView::Register(_routes, _settings);
        }

        AdminRequest Request(std::string method, std::string path, std::string body = {}) const
        {
            AdminRequest request;
            request.Method = std::move(method);
            request.Path = std::move(path);
            request.Body = std::move(body);
            request.RemoteAddress = "127.0.0.1";
            request.Authorization = std::string("Bearer ") + Token;
            return request;
        }

        static nlohmann::json Json(AdminResponse const& response)
        {
            return nlohmann::json::parse(response.Body, nullptr, false);
        }

        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::shared_ptr<MemorySettingStore> _store = std::make_shared<MemorySettingStore>();
        Settings _settings;
        AdminAuth _auth{ 10, 1.0 };
        AdminRouter _routes{ _auth };
        std::vector<std::vector<std::string>> _reveals;
    };

    std::string const FirstKey = "1:" + std::string(64, 'a');
    std::string const SecondKey = "2:" + std::string(64, 'b');

    bool HoldsKeyMaterial(std::string const& text)
    {
        return text.find("aaaaaaaa") != std::string::npos || text.find("bbbbbbbb") != std::string::npos;
    }
}

TEST_F(AdminSettingsViewTest, AnOutOfBoundsPutAnswers422AndChangesNothing)
{
    Open("World.UpdateInterval = 50\n");
    AdminResponse const response = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":0,"reason":"faster"})"));
    ASSERT_EQ(response.Status, 422) << response.Body;
    nlohmann::json const body = Json(response);
    EXPECT_EQ(body["error"], "invalid");
    EXPECT_NE(body["fields"]["value"].get<std::string>().find("from 1 to 10000 ms"), std::string::npos) << response.Body;
    ASSERT_EQ(body["errors"].size(), 1u);
    EXPECT_EQ(body["errors"][0]["code"], "out_of_bounds");
    EXPECT_EQ(body["errors"][0]["key"], "World.UpdateInterval");
    EXPECT_TRUE(_store->Writes.empty());
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 50u);
    EXPECT_EQ(_settings.GetPendingChangeCount(), 0u);

    AdminResponse const both = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":"fast"})"));
    ASSERT_EQ(both.Status, 422);
    nlohmann::json const named = Json(both);
    EXPECT_TRUE(named["fields"].contains("value")) << both.Body;
    EXPECT_TRUE(named["fields"].contains("reason")) << "every problem is named in one answer: " << both.Body;
    EXPECT_EQ(named["errors"][0]["code"], "wrong_type");

    AdminResponse const good = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster ticks"})"));
    ASSERT_EQ(good.Status, 200) << good.Body;
    EXPECT_EQ(Json(good)["value"], "100");
    EXPECT_EQ(Json(good)["changed"], true);
    EXPECT_EQ(Json(good)["layer"], "live");
    AdminResponse const again = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":"100","reason":"same"})"));
    ASSERT_EQ(again.Status, 200);
    EXPECT_EQ(Json(again)["changed"], false);
    EXPECT_EQ(_store->Writes.size(), 1u);
}

TEST_F(AdminSettingsViewTest, APutToAKeyTheEnvironmentSetsIsRefusedNamingTheVariable)
{
    Open("World.UpdateInterval = 50\nZone.UnloadDelay = 60\n", SettingApps::Game, { { "AMBROSE_WORLD_UPDATE_INTERVAL", "75" } }, { { "Zone.UnloadDelay", "90" } });
    AdminResponse const environment = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster"})"));
    ASSERT_EQ(environment.Status, 409) << environment.Body;
    nlohmann::json const body = Json(environment);
    EXPECT_EQ(body["error"], "setting_locked");
    EXPECT_EQ(body["layer"], "environment");
    EXPECT_NE(body["message"].get<std::string>().find("AMBROSE_WORLD_UPDATE_INTERVAL"), std::string::npos) << environment.Body;
    EXPECT_NE(body["origin"].get<std::string>().find("AMBROSE_WORLD_UPDATE_INTERVAL"), std::string::npos) << environment.Body;

    AdminResponse const override = _routes.Dispatch(Request("PUT", "/api/settings/Zone.UnloadDelay", R"({"value":30,"reason":"sooner"})"));
    ASSERT_EQ(override.Status, 409) << override.Body;
    EXPECT_EQ(Json(override)["layer"], "override");
    EXPECT_NE(Json(override)["message"].get<std::string>().find("command-line override"), std::string::npos);
    EXPECT_TRUE(_store->Writes.empty());
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 75u);

    nlohmann::json const listing = Json(_routes.Dispatch(Request("GET", "/api/settings")));
    for (nlohmann::json const& setting : listing["settings"])
        if (setting["key"] == "World.UpdateInterval")
        {
            EXPECT_EQ(setting["lock"]["layer"], "environment");
            EXPECT_EQ(setting["layer"], "environment");
        }
}

TEST_F(AdminSettingsViewTest, ABatchWithOneBadEntryAnswers422AndAppliesNone)
{
    Open("World.UpdateInterval = 50\n", SettingApps::Game, { { "AMBROSE_WORLD_HEARTBEAT", "30" } });
    AdminResponse const refused = _routes.Dispatch(Request("POST", "/api/settings/batch",
        R"({"reason":"tuning","entries":[{"key":"World.UpdateInterval","value":100},{"key":"Rate.XP.Quest","value":500},{"key":"World.Heartbeat","value":10}]})"));
    ASSERT_EQ(refused.Status, 422) << refused.Body;
    nlohmann::json const body = Json(refused);
    ASSERT_EQ(body["errors"].size(), 2u) << refused.Body;
    EXPECT_EQ(body["errors"][0]["key"], "Rate.XP.Quest");
    EXPECT_EQ(body["errors"][0]["code"], "out_of_bounds");
    EXPECT_EQ(body["errors"][1]["key"], "World.Heartbeat");
    EXPECT_EQ(body["errors"][1]["code"], "locked");
    EXPECT_EQ(body["errors"][1]["layer"], "environment");
    EXPECT_TRUE(body["fields"].contains("Rate.XP.Quest"));
    EXPECT_TRUE(_store->Writes.empty()) << "the good entry is not applied either";
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 50u);

    AdminResponse const landed = _routes.Dispatch(Request("POST", "/api/settings/batch",
        R"({"reason":"tuning","entries":[{"key":"World.UpdateInterval","value":100},{"key":"Rate.XP.Quest","value":2},{"key":"Rate.XP.Kill","value":1}]})"));
    ASSERT_EQ(landed.Status, 200) << landed.Body;
    nlohmann::json const applied = Json(landed);
    EXPECT_EQ(applied["changed"].size(), 2u);
    EXPECT_EQ(applied["unchanged"], nlohmann::json::array({ "Rate.XP.Kill" }));
    EXPECT_EQ(_store->Commits, 1u);
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 100u);
    EXPECT_EQ(_routes.CostOf("POST", "/api/settings/batch"), AdminSettingsView::BatchCost) << "a batch declares its cost as it registers";
}

TEST_F(AdminSettingsViewTest, VerifierKeysAreMaskedUnlessRevealedAndHistoryIsMasked)
{
    Open("Account.VerifierKeys = " + FirstKey + "\nAccount.VerifierActiveKey = 1\n", SettingApps::Login);
    AdminResponse const put = _routes.Dispatch(Request("PUT", "/api/settings/Account.VerifierKeys", nlohmann::json{ { "value", FirstKey + "," + SecondKey }, { "reason", "a second key" } }.dump()));
    ASSERT_EQ(put.Status, 200) << put.Body;
    EXPECT_FALSE(HoldsKeyMaterial(put.Body)) << put.Body;
    EXPECT_EQ(Json(put)["value"], "1:***,2:***");

    AdminResponse const masked = _routes.Dispatch(Request("GET", "/api/settings"));
    ASSERT_EQ(masked.Status, 200);
    EXPECT_FALSE(HoldsKeyMaterial(masked.Body)) << "no key material anywhere in the listing";
    nlohmann::json const listing = Json(masked);
    EXPECT_EQ(listing["schema"], AdminConfigView::SchemaVersion);
    EXPECT_EQ(listing["revealed"], false);
    bool found = false;
    for (nlohmann::json const& setting : listing["settings"])
        if (setting["key"] == "Account.VerifierKeys")
        {
            found = true;
            EXPECT_EQ(setting["value"], "1:***,2:***");
            EXPECT_EQ(setting["secret"], true);
            EXPECT_EQ(setting["visibility"], "secret");
            EXPECT_EQ(setting["edit"], "restricted");
            EXPECT_EQ(setting["layer"], "live");
            EXPECT_EQ(setting["declared"], true);
            EXPECT_EQ(setting["type"], "string");
            EXPECT_EQ(setting["apply"], "next_use");
            EXPECT_EQ(setting["category"], "Accounts");
            EXPECT_TRUE(setting["lock"].is_null());
            EXPECT_EQ(setting["persisted"], "1:***,2:***");
        }
    EXPECT_TRUE(found);
    EXPECT_TRUE(_reveals.empty()) << "a read that reveals nothing records nothing";

    AdminResponse const history = _routes.Dispatch(Request("GET", "/api/settings/Account.VerifierKeys/history"));
    ASSERT_EQ(history.Status, 200) << history.Body;
    EXPECT_FALSE(HoldsKeyMaterial(history.Body)) << history.Body;
    nlohmann::json const rows = Json(history);
    EXPECT_EQ(rows["visibility"], "secret");
    ASSERT_EQ(rows["entries"].size(), 1u);
    EXPECT_EQ(rows["entries"][0]["old"], "1:***");
    EXPECT_EQ(rows["entries"][0]["new"], "1:***,2:***");
    EXPECT_EQ(rows["entries"][0]["reason"], "a second key");
    for (SettingWrite const& write : _store->Writes)
    {
        EXPECT_FALSE(HoldsKeyMaterial(write.OldValue)) << "the audit row is masked at rest";
        EXPECT_FALSE(HoldsKeyMaterial(write.NewValue));
    }

    AdminRequest asking = Request("GET", "/api/settings");
    asking.QueryValues["reveal"] = "1";
    AdminResponse const revealed = _routes.Dispatch(asking);
    EXPECT_NE(revealed.Body.find(std::string(64, 'b')), std::string::npos) << "a caller who asks with the right sees the value";
    EXPECT_EQ(Json(revealed)["revealed"], true);
    ASSERT_EQ(_reveals.size(), 1u);
    EXPECT_EQ(_reveals[0], (std::vector<std::string>{ "Account.VerifierKeys" }));

    AdminRequest revealedHistory = Request("GET", "/api/settings/Account.VerifierKeys/history");
    revealedHistory.QueryValues["reveal"] = "1";
    EXPECT_FALSE(HoldsKeyMaterial(_routes.Dispatch(revealedHistory).Body)) << "history is never revealed";

    AdminRequest narrowed = asking;
    narrowed.ForwardedGrants = std::set<std::string, std::less<>>{ "settings.read" };
    EXPECT_FALSE(HoldsKeyMaterial(_routes.Dispatch(narrowed).Body)) << "a supervisor that did not forward the right keeps it masked";

    _routes.SetPermissionCheck([](AdminRequest const&, std::string_view permission)
    {
        return permission == "settings.secrets.read" ? PermissionVerdict::Forbidden : PermissionVerdict::Allowed;
    });
    EXPECT_FALSE(HoldsKeyMaterial(_routes.Dispatch(asking).Body)) << "asking without the right shows the mask";
    EXPECT_EQ(_reveals.size(), 1u) << "and records nothing";
}

TEST_F(AdminSettingsViewTest, ARestrictedKeyNeedsItsOwnRight)
{
    Open("Account.VerifierActiveKey = 0\n", SettingApps::Login);
    _routes.SetPermissionCheck([](AdminRequest const&, std::string_view permission)
    {
        return permission == AdminSettingsView::RestrictedPermission ? PermissionVerdict::Forbidden : PermissionVerdict::Allowed;
    });
    AdminResponse const put = _routes.Dispatch(Request("PUT", "/api/settings/Account.AllowPlainVerifiers", R"({"value":false,"reason":"every account is sealed"})"));
    EXPECT_EQ(put.Status, 403) << put.Body;
    AdminResponse const batch = _routes.Dispatch(Request("POST", "/api/settings/batch",
        R"({"reason":"tuning","entries":[{"key":"Account.PasswordMinLength","value":8},{"key":"Account.AllowPlainVerifiers","value":false}]})"));
    ASSERT_EQ(batch.Status, 422) << batch.Body;
    EXPECT_EQ(Json(batch)["errors"][0]["code"], "restricted");
    AdminResponse const normal = _routes.Dispatch(Request("PUT", "/api/settings/Account.PasswordMinLength", R"({"value":8,"reason":"longer passwords"})"));
    EXPECT_EQ(normal.Status, 200) << "a normal key needs only settings.edit: " << normal.Body;

    _routes.SetPermissionCheck({});
    AdminRequest relayed = Request("PUT", "/api/settings/Account.AllowPlainVerifiers", R"({"value":false,"reason":"sealed"})");
    relayed.ForwardedGrants = std::set<std::string, std::less<>>{ "settings.edit" };
    EXPECT_EQ(_routes.Dispatch(relayed).Status, 403) << "the supervisor's forwarded grants narrow the token";
    relayed.ForwardedGrants->insert(std::string(AdminSettingsView::RestrictedPermission));
    EXPECT_EQ(_routes.Dispatch(relayed).Status, 200);
}

TEST_F(AdminSettingsViewTest, ARelayedChangeIsAttributedToTheUserTheSupervisorNames)
{
    Open("World.UpdateInterval = 50\n");
    AdminRequest relayed = Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster"})");
    relayed.Actor = "user:3";
    relayed.ActorName = "Merle";
    ASSERT_EQ(_routes.Dispatch(relayed).Status, 200);
    AdminResponse const direct = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":120,"reason":"slower"})"));
    ASSERT_EQ(direct.Status, 200);

    nlohmann::json const rows = Json(_routes.Dispatch(Request("GET", "/api/settings/World.UpdateInterval/history")));
    ASSERT_EQ(rows["entries"].size(), 2u);
    EXPECT_EQ(rows["entries"][1]["who"], "Merle");
    EXPECT_EQ(rows["entries"][1]["source"], "panel");
    EXPECT_EQ(rows["entries"][0]["who"], "token");
    EXPECT_EQ(rows["entries"][0]["source"], "admin api");
}

TEST_F(AdminSettingsViewTest, BodiesThatAreNotAChangeAreRefusedFieldByField)
{
    Open("World.UpdateInterval = 50\n");
    EXPECT_EQ(_routes.Dispatch(Request("PUT", "/api/settings/No.Such.Setting", R"({"value":1,"reason":"x"})")).Status, 404);
    EXPECT_EQ(_routes.Dispatch(Request("GET", "/api/settings/No.Such.Setting/history")).Status, 404);
    EXPECT_EQ(_routes.Dispatch(Request("GET", "/api/settings/World.UpdateInterval")).Status, 404);
    EXPECT_EQ(_routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", "not json")).Status, 422);

    nlohmann::json const extra = Json(_routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":[1],"reason":"x","force":true})")));
    EXPECT_TRUE(extra["fields"].contains("force"));
    EXPECT_TRUE(extra["fields"].contains("value"));

    nlohmann::json const empty = Json(_routes.Dispatch(Request("POST", "/api/settings/batch", R"({"reason":"x","entries":[]})")));
    EXPECT_TRUE(empty["fields"].contains("entries"));
    nlohmann::json const shapeless = Json(_routes.Dispatch(Request("POST", "/api/settings/batch", R"({"entries":[{"key":"World.UpdateInterval"},{"key":"World.Heartbeat","value":5,"note":1}]})")));
    EXPECT_TRUE(shapeless["fields"].contains("reason"));
    EXPECT_EQ(shapeless["errors"].size(), 2u) << shapeless.dump();
    std::string const tooLong(Settings::MaxReasonBytes + 1, 'r');
    EXPECT_EQ(_routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", nlohmann::json{ { "value", 60 }, { "reason", tooLong } }.dump())).Status, 422);
    EXPECT_TRUE(_store->Writes.empty());

    EXPECT_EQ(AdminSettingsView::StatusOf(SettingResult::OutOfBounds), 422);
    EXPECT_EQ(AdminSettingsView::StatusOf(SettingResult::Locked), 409);
    EXPECT_EQ(AdminSettingsView::StatusOf(SettingResult::StoreFailed), 503);
}

TEST_F(AdminSettingsViewTest, AStoreThatFailsAnswers503AndAnUnopenedRegistrySaysSo)
{
    Open("World.UpdateInterval = 50\n");
    _store->FailWrites = true;
    AdminResponse const failed = _routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster"})"));
    EXPECT_EQ(failed.Status, 503) << failed.Body;
    EXPECT_EQ(Json(failed)["error"], "settings_store_failed");
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 50u);
}

TEST_F(AdminSettingsViewTest, AResetReturnsAKeyToItsConfigValueWithAnAuditRowOfItsOwn)
{
    Open("World.UpdateInterval = 50\n", SettingApps::Game, { { "AMBROSE_WORLD_HEARTBEAT", "30" } });
    ASSERT_EQ(_routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster"})")).Status, 200);
    EXPECT_EQ(_routes.Dispatch(Request("DELETE", "/api/settings/World.UpdateInterval", "{}")).Status, 422) << "a reset needs a reason too";

    AdminResponse const reset = _routes.Dispatch(Request("DELETE", "/api/settings/World.UpdateInterval", R"({"reason":"back to the file"})"));
    ASSERT_EQ(reset.Status, 200) << reset.Body;
    EXPECT_EQ(Json(reset)["value"], "50");
    EXPECT_EQ(Json(reset)["layer"], "config");
    EXPECT_EQ(Json(reset)["changed"], true);
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 50u);

    nlohmann::json const rows = Json(_routes.Dispatch(Request("GET", "/api/settings/World.UpdateInterval/history")));
    ASSERT_EQ(rows["entries"].size(), 2u);
    EXPECT_EQ(rows["entries"][0]["old"], "100");
    EXPECT_EQ(rows["entries"][0]["new"], "50");
    EXPECT_EQ(rows["entries"][0]["reason"], "back to the file");

    AdminResponse const again = _routes.Dispatch(Request("DELETE", "/api/settings/World.UpdateInterval", R"({"reason":"again"})"));
    ASSERT_EQ(again.Status, 200);
    EXPECT_EQ(Json(again)["changed"], false) << "a key with no live value has nothing to reset";
    EXPECT_EQ(_routes.Dispatch(Request("DELETE", "/api/settings/World.Heartbeat", R"({"reason":"x"})")).Status, 409);
    EXPECT_EQ(_routes.Dispatch(Request("DELETE", "/api/settings/No.Such.Setting", R"({"reason":"x"})")).Status, 404);
}

TEST_F(AdminSettingsViewTest, AResetARegisteredCheckRefusesAnswers422AndChangesNothing)
{
    Open("World.UpdateInterval = 50\n");
    ASSERT_TRUE(_settings.AddCheck("World.UpdateInterval", [](std::string_view value, Settings::ProposedValue const&) -> std::optional<std::string>
    {
        if (value == "50")
            return std::string("50 would drop a key a stored verifier still uses");
        return std::nullopt;
    }));
    ASSERT_EQ(_routes.Dispatch(Request("PUT", "/api/settings/World.UpdateInterval", R"({"value":100,"reason":"faster"})")).Status, 200);
    std::size_t const written = _store->Writes.size();

    AdminResponse const refused = _routes.Dispatch(Request("DELETE", "/api/settings/World.UpdateInterval", R"({"reason":"back to the file"})"));
    EXPECT_EQ(refused.Status, 422) << refused.Body;
    EXPECT_EQ(Json(refused)["message"], "50 would drop a key a stored verifier still uses");
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 100u);
    EXPECT_EQ(_store->Writes.size(), written) << "a refused reset writes no audit row";
    EXPECT_EQ(Json(_routes.Dispatch(Request("GET", "/api/settings/World.UpdateInterval/history")))["entries"].size(), 1u);
}

TEST_F(AdminSettingsViewTest, ADryRunSaysWhatABatchWouldChangeOrRefuseAndChangesNothing)
{
    Open("World.UpdateInterval = 50\n");
    AdminResponse const preview = _routes.Dispatch(Request("POST", "/api/settings/batch",
        R"({"dry_run":true,"entries":[{"key":"World.UpdateInterval","value":100},{"key":"Rate.XP.Kill","value":1}]})"));
    ASSERT_EQ(preview.Status, 200) << "a dry run needs no reason: " << preview.Body;
    nlohmann::json const body = Json(preview);
    EXPECT_EQ(body["dry_run"], true);
    ASSERT_EQ(body["changed"].size(), 1u);
    EXPECT_EQ(body["changed"][0]["key"], "World.UpdateInterval");
    EXPECT_EQ(body["changed"][0]["old"], "50");
    EXPECT_EQ(body["changed"][0]["new"], "100");
    EXPECT_EQ(body["unchanged"], nlohmann::json::array({ "Rate.XP.Kill" }));

    AdminResponse const refused = _routes.Dispatch(Request("POST", "/api/settings/batch",
        R"({"dry_run":true,"entries":[{"key":"World.UpdateInterval","value":100},{"key":"Rate.XP.Quest","value":500}]})"));
    ASSERT_EQ(refused.Status, 422) << refused.Body;
    EXPECT_EQ(Json(refused)["errors"][0]["key"], "Rate.XP.Quest");
    EXPECT_EQ(Json(refused)["errors"][0]["code"], "out_of_bounds");

    AdminResponse const unclear = _routes.Dispatch(Request("POST", "/api/settings/batch", R"({"dry_run":"yes","reason":"x","entries":[{"key":"World.UpdateInterval","value":100}]})"));
    EXPECT_EQ(unclear.Status, 422);
    EXPECT_TRUE(Json(unclear)["fields"].contains("dry_run")) << unclear.Body;
    EXPECT_FALSE(Json(unclear)["fields"].contains("reason")) << unclear.Body;
    EXPECT_TRUE(_store->Writes.empty()) << "nothing a dry run looked at was written";
    EXPECT_EQ(_settings.Get<uint32>("World.UpdateInterval"), 50u);
    EXPECT_EQ(_settings.GetPendingChangeCount(), 0u);
}

TEST_F(AdminSettingsViewTest, ARevealNamingOneKeyShowsAndRecordsOnlyThatOne)
{
    Open("Account.VerifierKeys = " + FirstKey + "\nAccount.VerifierActiveKey = 1\nLoginDatabaseInfo = 127.0.0.1;3306;ambrose;hunter2;ambrose_login\n", SettingApps::Login);
    AdminRequest asking = Request("GET", "/api/settings");
    asking.QueryValues["reveal"] = "Account.VerifierKeys";
    AdminResponse const answer = _routes.Dispatch(asking);
    ASSERT_EQ(answer.Status, 200);
    EXPECT_NE(answer.Body.find(std::string(64, 'a')), std::string::npos);
    EXPECT_EQ(answer.Body.find("hunter2"), std::string::npos) << "a secret the caller did not name stays masked";
    nlohmann::json const body = Json(answer);
    EXPECT_EQ(body["revealed_keys"], nlohmann::json::array({ "Account.VerifierKeys" }));
    for (nlohmann::json const& setting : body["settings"])
    {
        if (setting["key"] == "Account.VerifierKeys")
        {
            EXPECT_EQ(setting["revealed"], true);
        }
        if (setting["key"] == "LoginDatabaseInfo")
        {
            EXPECT_EQ(setting["revealed"], false);
        }
    }
    ASSERT_EQ(_reveals.size(), 1u);
    EXPECT_EQ(_reveals[0], (std::vector<std::string>{ "Account.VerifierKeys" }));

    asking.QueryValues["reveal"] = "0";
    EXPECT_EQ(_routes.Dispatch(asking).Body.find(std::string(64, 'a')), std::string::npos);
    EXPECT_EQ(_reveals.size(), 1u);
}
