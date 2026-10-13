/*
 * Project Ambrose by Imjustchico
 * Tests the versioned grant editor against a real panel listener and the no-escalation rule underneath it: saving grants takes the version the editor read and a stale one answers 409, an operator holding power.restart but not power.kill grants the first and is refused the second while a power.kill the target already holds stays in place, nobody edits an administrator's grants or their own, a grant change ends the target's open session at once, and the planner behind it refuses unknown keys, self edits and grants above the caller's own.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelGrantPolicy.h"
#include "PanelPermissions.h"
#include "PanelTestSession.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <string>

using namespace PanelTest;

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelGrantEditorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        void Start()
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
            ASSERT_TRUE(ClaimOwner(*_panel, _harness, _owner)) << "the owner claim link makes the owner";
        }

        std::string Invite(std::string const& scope, nlohmann::json const& extra = nlohmann::json::object())
        {
            nlohmann::json body = extra;
            body["scope"] = scope;
            AdminClientResponse const made = Send(*_panel, "POST", "/api/panel/invites", body, &_owner);
            EXPECT_EQ(made.Status, 201) << made.Body;
            return Json(made).value("token", std::string());
        }

        Browser MakeUser(std::string const& username, std::string const& role = "")
        {
            nlohmann::json extra;
            if (!role.empty())
                extra["role"] = role;
            std::string const token = Invite("gameserver", extra);
            AdminClientResponse const claimed = Send(*_panel, "POST", "/api/panel/invites/claim",
                { { "token", token }, { "username", username }, { "password", std::string(Password) } });
            EXPECT_EQ(claimed.Status, 200) << claimed.Body;
            Browser browser;
            EXPECT_TRUE(SignIn(*_panel, browser, username).Status == 200);
            EXPECT_FALSE(browser.Cookie.empty());
            return browser;
        }

        void Refresh(Browser& browser, std::string const& username)
        {
            browser = Browser{};
            EXPECT_TRUE(SignIn(*_panel, browser, username).Status == 200);
            EXPECT_FALSE(browser.Cookie.empty());
        }

        AdminClientResponse PutGrants(Browser const& by, nlohmann::json const& body, std::string const& ifMatch)
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "", _panel->IsSecure());
            AdminClientRequest request{ "PUT", "/api/panel/grant-sets", body.dump(), "application/json", "" };
            std::string cookie = by.Cookie;
            if (!cookie.empty())
                request.Headers.emplace_back("Cookie", cookie);
            request.Headers.emplace_back("Origin", std::string(_panel->IsSecure() ? "https" : "http") + "://127.0.0.1:" + std::to_string(_panel->GetPort()));
            if (!by.Csrf.empty())
                request.Headers.emplace_back("X-CSRF-Token", by.Csrf);
            if (!ifMatch.empty())
                request.Headers.emplace_back("If-Match", ifMatch);
            return client.Send(request, std::chrono::seconds(30));
        }

        nlohmann::json ReadGrants(int64 userId, std::string const& scope)
        {
            AdminClientResponse const read = Send(*_panel, "GET", fmt::format("/api/panel/grant-sets?user_id={}&scope={}", userId, scope), nullptr, &_owner);
            EXPECT_EQ(read.Status, 200) << read.Body;
            return Json(read);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
        Browser _owner;
    };
}

TEST_F(PanelGrantEditorTest, SavingTakesTheVersionReadAndAStaleOneConflicts)
{
    Start();
    Browser const target = MakeUser("edited");
    nlohmann::json const save = { { "user_id", target.UserId }, { "scope", "gameserver" }, { "add", { "power.restart" } } };

    AdminClientResponse const missing = PutGrants(_owner, save, "");
    EXPECT_EQ(missing.Status, 400) << missing.Body;
    EXPECT_EQ(Json(missing).value("error", std::string()), "version_required");

    AdminClientResponse const stale = PutGrants(_owner, save, "7");
    EXPECT_EQ(stale.Status, 409) << stale.Body;
    EXPECT_EQ(Json(stale).value("error", std::string()), "version_conflict");
    EXPECT_EQ(Json(stale).value("current_version", int64{ -1 }), 0);

    AdminClientResponse const first = PutGrants(_owner, save, "0");
    ASSERT_EQ(first.Status, 200) << first.Body;
    EXPECT_EQ(Json(first).value("version", int64{ 0 }), 1);
    EXPECT_TRUE(Json(first).value("changed", false));

    AdminClientResponse const again = PutGrants(_owner, save, "0");
    EXPECT_EQ(again.Status, 409) << again.Body;
    EXPECT_EQ(Json(again).value("current_version", int64{ 0 }), 1);

    AdminClientResponse const same = PutGrants(_owner, save, "1");
    EXPECT_EQ(same.Status, 200) << same.Body;
    EXPECT_FALSE(Json(same).value("changed", true));
    EXPECT_EQ(Json(same).value("version", int64{ 0 }), 1);
}

TEST_F(PanelGrantEditorTest, AnOperatorGrantsOnlyWhatTheyHold)
{
    Start();
    Browser const boss = MakeUser("boss", "admin");
    Browser op = MakeUser("oper");
    {
        std::string error;
        ASSERT_EQ(_panel->Users().SetRole(op.UserId, PanelRole::Admin, _owner.UserId, error), PanelUserResult::Ok) << error;
    }

    AdminClientResponse const granted = PutGrants(_owner, { { "user_id", op.UserId }, { "scope", "gameserver" }, { "add", { "power.restart" } } }, "0");
    ASSERT_EQ(granted.Status, 200) << granted.Body;
    Refresh(op, "oper");

    AdminClientResponse const tooMuch = PutGrants(op,
        { { "user_id", boss.UserId }, { "scope", "gameserver" }, { "add", { "power.restart", "console.raw" } } }, "0");
    EXPECT_EQ(tooMuch.Status, 403) << tooMuch.Body;
    EXPECT_EQ(Json(tooMuch).value("error", std::string()), "grant_not_held");

    AdminClientResponse const allowed = PutGrants(op, { { "user_id", boss.UserId }, { "scope", "gameserver" }, { "add", { "power.restart" } } }, "0");
    EXPECT_EQ(allowed.Status, 200) << allowed.Body;

    nlohmann::json const row = ReadGrants(boss.UserId, "gameserver");
    EXPECT_EQ(row["permissions"], nlohmann::json::array({ "power.restart" })) << row.dump();
}

TEST_F(PanelGrantEditorTest, AnOperatorCannotTouchAnAdministratorsGrantsOrTheirOwn)
{
    Start();
    Browser boss = MakeUser("boss", "admin");
    Browser op = MakeUser("oper");
    {
        std::string error;
        ASSERT_EQ(_panel->Users().SetRole(op.UserId, PanelRole::Admin, _owner.UserId, error), PanelUserResult::Ok) << error;
    }

    AdminClientResponse const granted = PutGrants(_owner, { { "user_id", op.UserId }, { "scope", "gameserver" }, { "add", { "power.restart" } } }, "0");
    ASSERT_EQ(granted.Status, 200) << granted.Body;
    AdminClientResponse const armed = PutGrants(_owner, { { "user_id", boss.UserId }, { "scope", "gameserver" }, { "add", { "console.raw" } } }, "0");
    ASSERT_EQ(armed.Status, 200) << armed.Body;
    Refresh(op, "oper");
    Refresh(boss, "boss");

    AdminClientResponse const demote = PutGrants(op, { { "user_id", boss.UserId }, { "scope", "gameserver" }, { "remove", { "console.raw" } } }, "1");
    EXPECT_EQ(demote.Status, 403) << demote.Body;
    EXPECT_EQ(Json(demote).value("error", std::string()), "grant_not_below_you");

    nlohmann::json const row = ReadGrants(boss.UserId, "gameserver");
    EXPECT_EQ(row["permissions"], nlohmann::json::array({ "console.raw" })) << row.dump();

    AdminClientResponse const self = PutGrants(op, { { "user_id", op.UserId }, { "scope", "gameserver" }, { "add", { "power.stop" } } }, "1");
    EXPECT_EQ(self.Status, 403) << self.Body;
    EXPECT_EQ(Json(self).value("error", std::string()), "grant_self_edit");
}

TEST_F(PanelGrantEditorTest, AGrantChangeEndsTheTargetsOpenSessionAtOnce)
{
    Start();
    Browser victim = MakeUser("victim");

    AdminClientResponse const before = Send(*_panel, "GET", "/api/panel/me", nullptr, &victim);
    ASSERT_EQ(before.Status, 200) << before.Body;

    AdminClientResponse const changed = PutGrants(_owner, { { "user_id", victim.UserId }, { "scope", "gameserver" }, { "add", { "power.restart" } } }, "0");
    ASSERT_EQ(changed.Status, 200) << changed.Body;

    AdminClientResponse const after = Send(*_panel, "GET", "/api/panel/me", nullptr, &victim);
    EXPECT_NE(after.Status, 200) << after.Body;
}

TEST(PanelGrantPolicyTest, AnEditAddsWhatTheCallerHoldsAndKeepsWhatTheyLack)
{
    std::set<std::string> const caller{ "power.restart" };
    PanelGrantCheck const check = PanelGrantPolicy::PlanEdit(1, 2, caller, {}, { "power.restart" }, {});
    EXPECT_EQ(check.Refusal, PanelGrantRefusal::None);
    EXPECT_EQ(check.Plan.Result, std::vector<std::string>{ "power.restart" });

    PanelGrantCheck const tooMuch = PanelGrantPolicy::PlanEdit(1, 2, caller, {}, { "power.kill" }, {});
    EXPECT_EQ(tooMuch.Refusal, PanelGrantRefusal::CannotGrant);
    EXPECT_EQ(tooMuch.Key, "power.kill");

    PanelGrantCheck const kept = PanelGrantPolicy::PlanEdit(1, 2, caller, { "power.kill" }, {}, { "power.kill" });
    EXPECT_EQ(kept.Refusal, PanelGrantRefusal::NotStrictSubset);

    std::set<std::string> const wider{ "power.restart", "power.stop" };
    PanelGrantCheck const untouched = PanelGrantPolicy::PlanEdit(1, 2, wider, { "power.restart" }, {}, { "power.kill" });
    EXPECT_EQ(untouched.Refusal, PanelGrantRefusal::None);
    EXPECT_EQ(untouched.Plan.Result, std::vector<std::string>{ "power.restart" });
    EXPECT_TRUE(untouched.Plan.Removed.empty());

    PanelGrantCheck const unknown = PanelGrantPolicy::PlanEdit(1, 2, wider, {}, { "power.everything" }, {});
    EXPECT_EQ(unknown.Refusal, PanelGrantRefusal::UnknownKey);

    PanelGrantCheck const self = PanelGrantPolicy::PlanEdit(1, 1, wider, {}, { "power.restart" }, {});
    EXPECT_EQ(self.Refusal, PanelGrantRefusal::SelfEdit);
}

TEST(PanelGrantPolicyTest, AnInviteCarriesNoMoreThanTheInviterHolds)
{
    std::set<std::string> const adminKeys{ "power.restart", "power.kill", "users.invite" };
    std::set<std::string> const adminAtScope{ "power.restart", "power.kill" };
    PanelGrantCheck const ok = PanelGrantPolicy::PlanInvite(1, std::nullopt, adminKeys, adminAtScope, "", { "power.restart" });
    EXPECT_EQ(ok.Refusal, PanelGrantRefusal::None);

    PanelGrantCheck const roleTooWide = PanelGrantPolicy::PlanInvite(1, std::nullopt, { "users.invite" }, {}, "admin", {});
    EXPECT_EQ(roleTooWide.Refusal, PanelGrantRefusal::CannotGrant);
    EXPECT_FALSE(roleTooWide.Key.empty());

    PanelGrantCheck const grantTooWide = PanelGrantPolicy::PlanInvite(1, std::nullopt, adminKeys, { "power.restart" }, "", { "power.kill" });
    EXPECT_EQ(grantTooWide.Refusal, PanelGrantRefusal::CannotGrant);
    EXPECT_EQ(grantTooWide.Key, "power.kill");

    PanelGrantCheck const self = PanelGrantPolicy::PlanInvite(1, 1, adminKeys, adminAtScope, "", { "power.restart" });
    EXPECT_EQ(self.Refusal, PanelGrantRefusal::SelfEdit);
}
