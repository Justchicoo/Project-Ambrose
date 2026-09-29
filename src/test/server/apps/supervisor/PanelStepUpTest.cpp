/*
 * Project Ambrose by Imjustchico
 * Tests step-up checks against a real panel listener with a clock the test moves: a danger action whose last check of who the caller is has passed the freshness window is answered 403 step_up_required, changes nothing and is recorded as asked for, while a fresh check made with a code lets the same request through and the audit row says what it authorized; a read needs no fresh check, a code already accepted cannot confirm a step-up, an operator without two-factor confirms with the password, a secret reveal asks every time, and a caller that carries no browser session cannot complete one, so a danger action it asks for fails closed.
 */

#include "AdminConfigView.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelTestSession.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace PanelTest;

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelStepUpTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
            _panel->TwoFactor().SetClock([this] { return _now; });
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        bool Age(int64 userId, std::chrono::minutes by)
        {
            std::string error;
            std::optional<PanelStore::Statement> update = _panel->Store().Prepare(
                "UPDATE panel_session SET checked_epoch_ms = checked_epoch_ms - ? WHERE user_id = ? AND ended_epoch_ms IS NULL", error);
            if (!update)
                return false;
            update->Bind(1, static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(by).count()));
            update->Bind(2, userId);
            return update->Run(error) && _panel->Store().Changed() > 0;
        }

        int64 CountResult(std::string const& name, AuditResult result)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT COUNT(*) FROM audit_event WHERE name = ? AND result = ?", error);
            if (!rows)
                return -1;
            rows->Bind(1, name);
            rows->Bind(2, PanelAudit::ToString(result));
            return rows->Step(error) ? rows->Int64(0) : -1;
        }

        nlohmann::json PropertiesOfLast(std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT properties FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            if (!rows)
                return nullptr;
            rows->Bind(1, name);
            return rows->Step(error) ? nlohmann::json::parse(rows->Text(0), nullptr, false) : nlohmann::json(nullptr);
        }

        std::string ReasonOfLast(std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT COALESCE(reason, '') FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            if (!rows)
                return {};
            rows->Bind(1, name);
            return rows->Step(error) ? rows->Text(0) : std::string();
        }

        AdminClientResponse Rename(Browser const& browser, std::string const& name)
        {
            return Send(*_panel, "PATCH", "/api/panel/settings", { { "values", { { "Panel.Name", name } } } }, &browser);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
        int64 _now = 1800000000;
    };
}

TEST_F(PanelStepUpTest, ADangerActionPastTheFreshnessWindowAsksAgainAndChangesNothing)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_TRUE(Enroll(*_panel, owner, _now).has_value());
    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(6)));

    AdminClientResponse const asked = Rename(owner, "Somewhere Else");
    ASSERT_EQ(asked.Status, 403) << asked.Body;
    nlohmann::json const body = Json(asked);
    EXPECT_EQ(body["error"], "step_up_required");
    EXPECT_EQ(body["permission"], "panel.settings");
    EXPECT_EQ(body["methods"], nlohmann::json::array({ "totp", "recovery_code" }));
    EXPECT_EQ(body["window_seconds"], 300);

    EXPECT_EQ(_panel->Settings().ValueOf("Panel.Name"), "Ambrose") << "nothing changed";
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:settings.changed"), 0);
    EXPECT_EQ(CountResult("panel:step_up.required", AuditResult::Refused), 1);
    EXPECT_EQ(CountResult("panel:step_up.used", AuditResult::Succeeded), 0);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &owner).Status, 200) << "asking again does not end the session";
}

TEST_F(PanelStepUpTest, AFreshCheckAuthorizesTheActionAndTheAuditRowSaysWhat)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(6)));
    ASSERT_EQ(Rename(owner, "Somewhere Else").Status, 403);

    _now += Totp::StepSeconds;
    AdminClientResponse const checked = Send(*_panel, "POST", "/api/panel/step-up", { { "code", CodeAt(enrolled->Secret, _now) }, { "for", "change the panel's settings" } }, &owner);
    ASSERT_EQ(checked.Status, 200) << checked.Body;
    EXPECT_EQ(Json(checked)["window_seconds"], 300);
    EXPECT_EQ(PropertiesOfLast("panel:step_up.checked")["for"], "change the panel's settings");
    EXPECT_EQ(PropertiesOfLast("panel:step_up.checked")["method"], "totp");

    AdminClientResponse const renamed = Rename(owner, "Somewhere Else");
    ASSERT_EQ(renamed.Status, 200) << renamed.Body;
    EXPECT_EQ(_panel->Settings().ValueOf("Panel.Name"), "Somewhere Else");
    EXPECT_EQ(CountResult("panel:step_up.used", AuditResult::Succeeded), 1);
    nlohmann::json const used = PropertiesOfLast("panel:step_up.used");
    EXPECT_EQ(used["permission"], "panel.settings") << used.dump();
    EXPECT_EQ(used["method"], "PATCH");
    EXPECT_EQ(used["path"], "/api/panel/settings");
    EXPECT_TRUE(used["checked_epoch_ms"].is_number_integer()) << "the row says when the check it rests on was made";
    EXPECT_EQ(ReasonOfLast("panel:step_up.used"), "PATCH /api/panel/settings");
}

TEST_F(PanelStepUpTest, AReadNeedsNoFreshCheck)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(60)));
    AdminClientResponse const read = Send(*_panel, "GET", "/api/panel/settings", nullptr, &owner);
    EXPECT_EQ(read.Status, 200) << read.Body;
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:step_up.required"), 0);
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:step_up.used"), 0);
}

TEST_F(PanelStepUpTest, AStepUpCodeCannotBeOneAlreadyAccepted)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(6)));

    AdminClientResponse const replayed = Send(*_panel, "POST", "/api/panel/step-up", { { "code", CodeAt(enrolled->Secret, _now) } }, &owner);
    EXPECT_EQ(replayed.Status, 403) << "the code that turned two-factor on cannot confirm a step-up";
    EXPECT_EQ(ErrorOf(replayed), "step_up_refused");
    EXPECT_EQ(ReasonOfLast("panel:step_up.checked"), "replayed");
    EXPECT_EQ(CountResult("panel:step_up.checked", AuditResult::Refused), 1);
    EXPECT_EQ(Rename(owner, "Somewhere Else").Status, 403) << "a refused check leaves the session as stale as it was";

    AdminClientResponse const password = Send(*_panel, "POST", "/api/panel/step-up", { { "password", Password } }, &owner);
    EXPECT_EQ(password.Status, 422) << "an operator with two-factor confirms with a code, not the password alone";

    _now += Totp::StepSeconds;
    std::string const next = CodeAt(enrolled->Secret, _now);
    ASSERT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", next } }, &owner).Status, 200);
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", next } }, &owner).Status, 403) << "and a code a step-up took works only once";
}

TEST_F(PanelStepUpTest, AnOperatorWithoutTwoFactorConfirmsWithThePassword)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(6)));
    AdminClientResponse const asked = Rename(owner, "Somewhere Else");
    ASSERT_EQ(asked.Status, 403);
    EXPECT_EQ(Json(asked)["methods"], nlohmann::json::array({ "password" }));

    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", "123456" } }, &owner).Status, 422) << "without two-factor the check is the password";
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "password", "not the password" } }, &owner).Status, 403);
    EXPECT_EQ(ReasonOfLast("panel:step_up.checked"), "the password was wrong");
    ASSERT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "password", Password } }, &owner).Status, 200);
    EXPECT_EQ(Rename(owner, "Somewhere Else").Status, 200);
}

TEST_F(PanelStepUpTest, ASecretRevealAsksForAFreshCheckEveryTime)
{
    std::vector<std::vector<std::string>> recorded;
    AdminConfigView::Register(_panel->Routes(), *_config, {}, nullptr, [&recorded](AdminRequest const&, std::vector<std::string> const& keys) { recorded.push_back(keys); });
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    EXPECT_EQ(Send(*_panel, "GET", "/api/settings", nullptr, &owner).Status, 200);
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:step_up.used"), 0) << "a read that reveals nothing asks nothing";

    AdminClientResponse const fresh = Send(*_panel, "GET", "/api/settings?reveal=1", nullptr, &owner);
    ASSERT_EQ(fresh.Status, 200) << fresh.Body;
    EXPECT_EQ(PropertiesOfLast("panel:step_up.used")["permission"], "settings.secrets.read") << "a reveal is recorded with the check it rested on";
    EXPECT_EQ(PropertiesOfLast("panel:step_up.used")["method"], "GET");

    ASSERT_TRUE(Age(owner.UserId, std::chrono::minutes(6)));
    AdminClientResponse const stale = Send(*_panel, "GET", "/api/settings?reveal=1", nullptr, &owner);
    ASSERT_EQ(stale.Status, 403) << stale.Body;
    EXPECT_EQ(ErrorOf(stale), "step_up_required");
    EXPECT_EQ(Json(stale)["permission"], "settings.secrets.read");
    EXPECT_EQ(CountResult("panel:step_up.required", AuditResult::Refused), 1);
}

TEST_F(PanelStepUpTest, ACallerWithoutABrowserSessionCannotCompleteAStepUp)
{
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/power";
    request.RemoteAddress = "127.0.0.1";
    request.Authorization = "Bearer a-personal-key-names-its-operator";
    request.Principal = "user:" + std::to_string(owner.UserId);
    std::optional<AdminResponse> const always = _panel->StepUpCheck(request, "power.kill", StepUpWhen::Always);
    ASSERT_TRUE(always.has_value()) << "a caller the panel cannot ask again is refused, not let through";
    EXPECT_EQ(always->Status, 403);
    EXPECT_EQ(nlohmann::json::parse(always->Body)["error"], "step_up_unavailable");
    EXPECT_TRUE(_panel->StepUpCheck(request, "power.kill", StepUpWhen::Changing).has_value()) << "and so is a change a danger permission guards";
    EXPECT_FALSE(_panel->StepUpCheck(request, "power.restart", StepUpWhen::Changing).has_value()) << "a permission that is not a danger one asks nothing";

    AdminRequest read = request;
    read.Method = "GET";
    EXPECT_FALSE(_panel->StepUpCheck(read, "power.kill", StepUpWhen::Changing).has_value()) << "nor does a read";
}
