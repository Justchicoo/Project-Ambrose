/*
 * Project Ambrose by Imjustchico
 * Tests two-factor sign-in against a real panel listener with a clock the test moves: a correct password alone opens no session, a wrong code is refused and the challenge dies after its attempts, a code accepted once is refused the second time on sign-in, on enabling and on a step-up check, and each recovery code works once, typed in any case with or without its dash, and never for another operator; requiring it for everyone sends a signed-in operator without it to enrollment on their next request, whether it comes as a route, a socket or a request carrying the operator without a cookie, while bearer-token requests are refused too, and the routes that turn it on still answer, each requirement covers the operators it names, and a requirement the panel does not know stops the start and a reload that names one keeps the old; disabling needs the password and a current code, is refused while the requirement covers the operator, and ends every other session; moving to another authenticator app takes a current code or recovery code from the app in use as well as the password and a code from the new app, after which the old app and the old recovery codes sign nobody in; a recovery code appears in no answer, log line, audit row or stored file after the answer that issued it, which only its keyed hash does; and the one-time password link, the second factor's own throttle, the window, a pending secret and the console reset each behave as the panel promises.
 */

#include "AdminTestClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelTestSession.h"
#include "RecoveryCode.h"
#include "SHA256.h"
#include "Base64.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace PanelTest;

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    std::string ReadAll(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    class PanelTwoFactorTest : public testing::Test
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

        void Start(std::string const& extra = "")
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n" + extra)).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
            _panel->TwoFactor().SetClock([this] { return _now; });
        }

        bool Reload(std::string const& extra)
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            if (!_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n" + extra)).Succeeded())
                return false;
            return _panel->Reload(*_config);
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

        std::string ReasonOfLast(std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT COALESCE(reason, '') FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            if (!rows)
                return {};
            rows->Bind(1, name);
            return rows->Step(error) ? rows->Text(0) : std::string();
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
        int64 _now = 1800000000;
    };
}

TEST_F(PanelTwoFactorTest, APasswordWithoutAValidCodeOpensNoSession)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += Totp::StepSeconds;

    Browser second;
    AdminClientResponse const asked = SignIn(*_panel, second, "owner");
    ASSERT_EQ(asked.Status, 200) << asked.Body;
    nlohmann::json const body = Json(asked);
    EXPECT_TRUE(body.value("second_factor", false));
    EXPECT_FALSE(body.contains("csrf")) << "a password alone is handed no CSRF token";
    EXPECT_FALSE(body.contains("user"));
    EXPECT_TRUE(SessionCookie(asked).empty()) << "a password alone opens no session";
    ASSERT_FALSE(second.Challenge.empty());
    EXPECT_NE(asked.Head.find("HttpOnly"), std::string::npos);
    EXPECT_NE(asked.Head.find("SameSite=Strict"), std::string::npos);
    EXPECT_NE(asked.Head.find("Max-Age=300"), std::string::npos);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &second).Status, 401) << "the challenge cookie is not a session";
    EXPECT_EQ(CountResult("panel:session.challenged", AuditResult::Succeeded), 1);

    std::string const wrong = WrongCode(enrolled->Secret, _now);
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        AdminClientResponse const refused = SecondFactor(*_panel, second, { { "code", wrong } });
        EXPECT_EQ(refused.Status, 401) << attempt;
        EXPECT_EQ(ErrorOf(refused), "second_factor_refused") << attempt;
    }
    AdminClientResponse const late = SecondFactor(*_panel, second, { { "code", CodeAt(enrolled->Secret, _now) } });
    EXPECT_EQ(late.Status, 401) << late.Body;
    EXPECT_EQ(ErrorOf(late), "challenge_expired") << "after its attempts the challenge is gone, whatever code comes next";
    EXPECT_EQ(CountResult("panel:session.second_factor_refused", AuditResult::Refused), 5);
    EXPECT_EQ(CountResult("panel:session.opened", AuditResult::Succeeded), 1) << "only the claim opened a session";

    Browser third;
    ASSERT_EQ(SignIn(*_panel, third, "owner").Status, 200);
    AdminClientResponse const signedIn = SecondFactor(*_panel, third, { { "code", CodeAt(enrolled->Secret, _now) } });
    ASSERT_EQ(signedIn.Status, 200) << signedIn.Body;
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &third).Status, 200);
    EXPECT_NE(ReasonOfLast("panel:session.opened").find("TOTP code"), std::string::npos);
}

TEST_F(PanelTwoFactorTest, ACodeAcceptedOnceIsRefusedTheSecondTime)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());

    Browser first;
    ASSERT_EQ(SignIn(*_panel, first, "owner").Status, 200);
    AdminClientResponse const replayed = SecondFactor(*_panel, first, { { "code", CodeAt(enrolled->Secret, _now) } });
    EXPECT_EQ(replayed.Status, 401) << "the code that turned two-factor on cannot sign in";
    EXPECT_EQ(ReasonOfLast("panel:session.second_factor_refused"), "replayed");

    _now += Totp::StepSeconds;
    std::string const next = CodeAt(enrolled->Secret, _now);
    ASSERT_EQ(SecondFactor(*_panel, first, { { "code", next } }).Status, 200);

    Browser second;
    ASSERT_EQ(SignIn(*_panel, second, "owner").Status, 200);
    EXPECT_EQ(SecondFactor(*_panel, second, { { "code", next } }).Status, 401) << "a code accepted once is refused the second time";

    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", next } }, &first).Status, 403) << "nor does it confirm a step-up";
    _now += Totp::StepSeconds;
    std::string const later = CodeAt(enrolled->Secret, _now);
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", later }, { "for", "change panel settings" } }, &first).Status, 200);
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/step-up", { { "code", later } }, &first).Status, 403);
    EXPECT_EQ(SecondFactor(*_panel, second, { { "code", later } }).Status, 401) << "and a code a step-up took cannot sign in either";
}

TEST_F(PanelTwoFactorTest, EachRecoveryCodeWorksOnce)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    ASSERT_EQ(enrolled->Codes.size(), RecoveryCode::Count);
    for (std::string const& code : enrolled->Codes)
        EXPECT_EQ(code.size(), RecoveryCode::Length + 1) << code;

    std::string lowered = enrolled->Codes[0];
    for (char& c : lowered)
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    Browser first;
    ASSERT_EQ(SignIn(*_panel, first, "owner").Status, 200);
    AdminClientResponse const used = SecondFactor(*_panel, first, { { "recovery_code", lowered } });
    ASSERT_EQ(used.Status, 200) << used.Body;
    EXPECT_NE(ReasonOfLast("panel:session.opened").find("recovery code"), std::string::npos);

    Browser second;
    ASSERT_EQ(SignIn(*_panel, second, "owner").Status, 200);
    EXPECT_EQ(SecondFactor(*_panel, second, { { "recovery_code", enrolled->Codes[0] } }).Status, 401) << "a recovery code works once";
    EXPECT_EQ(ReasonOfLast("panel:session.second_factor_refused"), "replayed");

    std::string bare = enrolled->Codes[1];
    bare.erase(std::remove(bare.begin(), bare.end(), '-'), bare.end());
    EXPECT_EQ(SecondFactor(*_panel, second, { { "recovery_code", bare } }).Status, 200) << "without its dash it is the same code";
    AdminClientResponse const state = Send(*_panel, "GET", "/api/panel/me/two-factor", nullptr, &second);
    ASSERT_EQ(state.Status, 200) << state.Body;
    EXPECT_EQ(Json(state)["recovery_codes_left"], 8);

    std::string error;
    int64 helperId = 0;
    ASSERT_EQ(_panel->Users().Create("helper", "another good password", false, false, &helperId, error), PanelUserResult::Ok) << error;
    Browser helper;
    ASSERT_EQ(SignIn(*_panel, helper, "helper", "another good password").Status, 200);
    std::optional<Enrolled> const helperEnrolled = Enroll(*_panel, helper, _now, "another good password");
    ASSERT_TRUE(helperEnrolled.has_value());
    Browser third;
    ASSERT_EQ(SignIn(*_panel, third, "owner").Status, 200);
    EXPECT_EQ(SecondFactor(*_panel, third, { { "recovery_code", helperEnrolled->Codes[0] } }).Status, 401) << "another operator's code signs nobody else in";
    EXPECT_EQ(SecondFactor(*_panel, third, { { "recovery_code", "not-a-code" } }).Status, 401);
}

TEST_F(PanelTwoFactorTest, RequiringItForEveryoneSendsAUserWithoutItToEnrollmentOnTheirNextRequest)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_EQ(Send(*_panel, "GET", "/api/panel/settings", nullptr, &owner).Status, 200);

    ASSERT_TRUE(Reload("Panel.TwoFactorRequired = everyone\n"));
    EXPECT_EQ(_panel->TwoFactorSettings().Required, PanelTwoFactorPolicy::Everyone);

    AdminClientResponse const held = Send(*_panel, "GET", "/api/panel/settings", nullptr, &owner);
    EXPECT_EQ(held.Status, 403) << held.Body;
    EXPECT_EQ(ErrorOf(held), "two_factor_required");
    EXPECT_EQ(ErrorOf(Send(*_panel, "GET", "/api/panel/permissions", nullptr, &owner)), "two_factor_required");

    AdminClientResponse const me = Send(*_panel, "GET", "/api/panel/me", nullptr, &owner);
    ASSERT_EQ(me.Status, 200) << "the enrollment routes still answer";
    EXPECT_TRUE(Json(me)["two_factor_required"].get<bool>());
    EXPECT_FALSE(Json(me)["two_factor"].get<bool>());
    AdminClientResponse const probe = Send(*_panel, "GET", "/api/panel/session", nullptr, &owner);
    ASSERT_EQ(probe.Status, 200);
    EXPECT_TRUE(Json(probe)["user"]["two_factor_required"].get<bool>()) << "the page knows before it asks for anything else";
    AdminClientResponse const state = Send(*_panel, "GET", "/api/panel/me/two-factor", nullptr, &owner);
    ASSERT_EQ(state.Status, 200);
    EXPECT_TRUE(Json(state)["required"].get<bool>());

    ASSERT_TRUE(Enroll(*_panel, owner, _now).has_value()) << "setting up and turning it on are open to an operator who must";
    AdminClientResponse const settings = Send(*_panel, "GET", "/api/panel/settings?group=security", nullptr, &owner);
    ASSERT_EQ(settings.Status, 200) << "and once it is on everything answers again, in the same session";
    bool shown = false;
    nlohmann::json const rows = Json(settings);
    ASSERT_TRUE(rows.is_object() && rows.contains("settings")) << settings.Body;
    for (nlohmann::json const& row : rows["settings"])
    {
        if (row["key"] != "Panel.TwoFactorRequired")
            continue;
        shown = true;
        EXPECT_EQ(row["value"], "everyone");
        EXPECT_EQ(row["layer"], "config");
        EXPECT_TRUE(row["locked"].get<bool>());
    }
    EXPECT_TRUE(shown) << "the settings page shows the requirement in force and the layer that set it";
}

TEST_F(PanelTwoFactorTest, ASocketUpgradeIsHeldToTheRequirement)
{
    Start("Panel.TwoFactorRequired = everyone\n");
    _panel->AddSocket({ "/api/panel/probe-socket", "status.read", {}, {}, {} });
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::string const port = std::to_string(_panel->GetPort());
    auto const upgrade = [&]
    {
        return AdminTest::Send(_panel->GetPort(), "GET /api/panel/probe-socket HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nOrigin: http://127.0.0.1:" + port + "\r\nCookie: " + owner.Cookie + "\r\n\r\n");
    };
    AdminTest::HttpReply const held = upgrade();
    EXPECT_EQ(held.Status, 403) << held.Head;
    EXPECT_NE(held.Body.find("two_factor_required"), std::string::npos) << held.Body;

    ASSERT_TRUE(Enroll(*_panel, owner, _now).has_value());
    EXPECT_EQ(upgrade().Status, 101) << "with two-factor on the same socket opens";
}

TEST_F(PanelTwoFactorTest, APanelTokenRequestAnswers403TwoFactorRequired)
{
    Start("Panel.TwoFactorRequired = everyone\nPanel.Token = panel-test-token-with-at-least-thirty-two-bytes\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    AdminTest::HttpReply const apiTokenRequest = AdminTest::Ask(_panel->GetPort(), "GET", "/api/panel/settings", "panel-test-token-with-at-least-thirty-two-bytes");
    EXPECT_EQ(apiTokenRequest.Status, 403) << apiTokenRequest.Body;
    EXPECT_EQ(nlohmann::json::parse(apiTokenRequest.Body)["error"], "two_factor_required");

    AdminRequest request;
    request.Method = "GET";
    request.Path = "/api/panel/settings";
    request.RemoteAddress = "127.0.0.1";
    request.Authorization = "Bearer a-personal-key-names-its-operator";
    request.Principal = "user:" + std::to_string(owner.UserId);
    std::optional<AdminResponse> const held = _panel->Admit(request);
    ASSERT_TRUE(held.has_value()) << "the requirement follows the operator, not the cookie";
    EXPECT_EQ(held->Status, 403);
    EXPECT_EQ(nlohmann::json::parse(held->Body)["error"], "two_factor_required");

    AdminRequest unknown = request;
    unknown.Principal = "key:7";
    std::optional<AdminResponse> const heldKey = _panel->Admit(unknown);
    ASSERT_TRUE(heldKey.has_value()) << "a caller the panel cannot tie to an enrolled operator is held back";
    EXPECT_EQ(heldKey->Status, 403);
    EXPECT_EQ(nlohmann::json::parse(heldKey->Body)["error"], "two_factor_required");
    AdminRequest token = request;
    token.Principal = "token";
    std::optional<AdminResponse> const heldToken = _panel->Admit(token);
    ASSERT_TRUE(heldToken.has_value()) << "a bearer token cannot bypass a policy that holds every request until enrollment";
    EXPECT_EQ(heldToken->Status, 403);
    EXPECT_EQ(nlohmann::json::parse(heldToken->Body)["error"], "two_factor_required");

    ASSERT_TRUE(Enroll(*_panel, owner, _now).has_value());
    EXPECT_FALSE(_panel->Admit(request).has_value());
}

TEST_F(PanelTwoFactorTest, EachRequirementCoversWhomItNames)
{
    auto const as = [](PanelRole role)
    {
        PanelUser user;
        user.Id = 5;
        user.Role = role;
        user.IsOwner = role == PanelRole::Owner;
        return user;
    };
    PanelGrant danger;
    danger.UserId = 5;
    danger.App = "gameserver";
    danger.Permission = "power.kill";
    PanelGrant plain = danger;
    plain.Permission = "power.restart";
    std::vector<PanelGrant> const none;

    for (PanelRole const role : PanelPermissions::Roles())
    {
        EXPECT_FALSE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::None, as(role), none));
        EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Everyone, as(role), none));
    }
    EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Admins, as(PanelRole::Owner), none));
    EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Admins, as(PanelRole::Admin), none));
    EXPECT_FALSE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Admins, as(PanelRole::Operator), none));
    EXPECT_FALSE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Admins, as(PanelRole::Viewer), { danger })) << "a danger grant does not make somebody an admin";
    EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Danger, as(PanelRole::Owner), none));
    EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Danger, as(PanelRole::Admin), none));
    EXPECT_FALSE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Danger, as(PanelRole::Operator), none));
    EXPECT_FALSE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Danger, as(PanelRole::Viewer), { plain }));
    EXPECT_TRUE(PanelTwoFactor::Covers(PanelTwoFactorPolicy::Danger, as(PanelRole::Viewer), { danger })) << "one danger grant is enough";

    EXPECT_EQ(PanelTwoFactorSettings::ParsePolicy(" Everyone "), std::optional<PanelTwoFactorPolicy>(PanelTwoFactorPolicy::Everyone));
    EXPECT_EQ(PanelTwoFactorSettings::ParsePolicy("ADMINS"), std::optional<PanelTwoFactorPolicy>(PanelTwoFactorPolicy::Admins));
    EXPECT_FALSE(PanelTwoFactorSettings::ParsePolicy("owners").has_value());
}

TEST_F(PanelTwoFactorTest, AnUnknownRequirementStopsTheStartAndARefusedReloadKeepsTheOldOne)
{
    {
        ConfigMgr config(NoEnvironment());
        ASSERT_TRUE(config.LoadInitial(_directory.Write("unknown.conf", "Panel.Enable = 1\nPanel.Port = 0\nPanel.TwoFactorRequired = sometimes\n")).Succeeded());
        Panel refused(_harness.GetLog(), _directory.Path() / "refused", _directory.Path());
        std::string error;
        EXPECT_FALSE(refused.Start(config, error));
        EXPECT_NE(error.find("Panel.TwoFactorRequired"), std::string::npos) << error;
        EXPECT_FALSE(refused.IsRunning());
        refused.Stop();
    }

    Start("Panel.TwoFactorRequired = admins\n");
    EXPECT_EQ(_panel->TwoFactorSettings().Required, PanelTwoFactorPolicy::Admins);
    EXPECT_FALSE(Reload("Panel.TwoFactorRequired = most\n"));
    EXPECT_EQ(_panel->TwoFactorSettings().Required, PanelTwoFactorPolicy::Admins) << "a refused reload keeps the old requirement";
    EXPECT_TRUE(_panel->IsRunning());

    EXPECT_TRUE(Reload("Panel.TwoFactorRequired = EVERYONE\nPanel.StepUpMinutes = 500\nPanel.TwoFactorWindow = 2\n"));
    PanelTwoFactorSettings const applied = _panel->TwoFactorSettings();
    EXPECT_EQ(applied.Required, PanelTwoFactorPolicy::Everyone);
    EXPECT_EQ(applied.StepUpWindow, std::chrono::minutes(120)) << "a value past its bound is held to it";
    EXPECT_EQ(_panel->TwoFactor().GetWindow(), 2u) << "the window applies live";
    bool named = false;
    for (std::string const& line : _harness.Store().Texts("Capture"))
        named = named || line.find("Panel.StepUpMinutes is 500") != std::string::npos;
    EXPECT_TRUE(named);
}

TEST_F(PanelTwoFactorTest, DisablingWithoutACurrentCodeIsRefusedAndWithOneEndsTheOtherSessions)
{
    Start();
    Browser first;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, first));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, first, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += Totp::StepSeconds;
    std::string const used = CodeAt(enrolled->Secret, _now);
    Browser second;
    ASSERT_EQ(SignIn(*_panel, second, "owner").Status, 200);
    ASSERT_EQ(SecondFactor(*_panel, second, { { "code", used } }).Status, 200);
    ASSERT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &second).Status, 200);

    AdminClientResponse const passwordOnly = Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", Password } }, &first);
    EXPECT_EQ(passwordOnly.Status, 422) << passwordOnly.Body;
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", Password }, { "code", WrongCode(enrolled->Secret, _now) } }, &first).Status, 403);
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", Password }, { "code", used } }, &first).Status, 403) << "a replayed code is not a current one";
    _now += Totp::StepSeconds;
    std::string const current = CodeAt(enrolled->Secret, _now);
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", "not the password" }, { "code", current } }, &first).Status, 403);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &second).Status, 200) << "nothing refused ended anything";

    AdminClientResponse const disabled = Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", Password }, { "code", current } }, &first);
    ASSERT_EQ(disabled.Status, 200) << disabled.Body;
    EXPECT_FALSE(Json(disabled)["user"]["two_factor"].get<bool>());
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &second).Status, 401) << "every other session of that operator has ended";
    AdminClientResponse const still = Send(*_panel, "GET", "/api/panel/me", nullptr, &first);
    ASSERT_EQ(still.Status, 200) << "the session that turned it off stays";
    EXPECT_FALSE(Json(still)["two_factor"].get<bool>());

    EXPECT_EQ(CountResult("user:two_factor.disabled", AuditResult::Succeeded), 1);
    EXPECT_EQ(CountResult("user:two_factor.disabled", AuditResult::Refused), 3);
    std::string error;
    std::optional<PanelStore::Statement> left = _panel->Store().Prepare(
        "SELECT (SELECT COUNT(*) FROM panel_two_factor WHERE user_id = ?1) + (SELECT COUNT(*) FROM panel_recovery_code WHERE user_id = ?1)", error);
    ASSERT_TRUE(left.has_value()) << error;
    left->Bind(1, first.UserId);
    ASSERT_TRUE(left->Step(error)) << error;
    EXPECT_EQ(left->Int64(0), 0) << "the secret and every recovery code are deleted";

    Browser again;
    AdminClientResponse const plain = SignIn(*_panel, again, "owner");
    ASSERT_EQ(plain.Status, 200);
    EXPECT_FALSE(Json(plain).value("second_factor", false)) << "a password signs in alone again";
}

TEST_F(PanelTwoFactorTest, DisablingIsRefusedWhileTheRequirementCoversTheUser)
{
    Start("Panel.TwoFactorRequired = admins\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += Totp::StepSeconds;
    AdminClientResponse const refused = Send(*_panel, "POST", "/api/panel/me/two-factor/disable", { { "password", Password }, { "code", CodeAt(enrolled->Secret, _now) } }, &owner);
    EXPECT_EQ(refused.Status, 409) << refused.Body;
    EXPECT_EQ(ErrorOf(refused), "two_factor_required");
    AdminClientResponse const me = Send(*_panel, "GET", "/api/panel/me", nullptr, &owner);
    ASSERT_EQ(me.Status, 200);
    EXPECT_TRUE(Json(me)["two_factor"].get<bool>());
}

TEST_F(PanelTwoFactorTest, ARecoveryCodeIsShownOnceAndTheStoreHoldsOnlyItsKeyedHash)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    ASSERT_EQ(enrolled->Codes.size(), RecoveryCode::Count);

    std::vector<std::string> spellings;
    std::vector<std::string> normalized;
    for (std::string const& code : enrolled->Codes)
    {
        std::optional<std::string> const bare = RecoveryCode::Normalize(code);
        ASSERT_TRUE(bare.has_value()) << code;
        normalized.push_back(*bare);
        spellings.push_back(code);
        spellings.push_back(*bare);
    }
    for (std::string const& spelling : spellings)
        EXPECT_NE(enrolled->Body.find(spelling.substr(0, 5)), std::string::npos) << "the answer that issued them shows them once";

    Browser signing;
    ASSERT_EQ(SignIn(*_panel, signing, "owner").Status, 200);
    AdminClientResponse const used = SecondFactor(*_panel, signing, { { "recovery_code", enrolled->Codes[3] } });
    ASSERT_EQ(used.Status, 200);
    std::vector<std::string> const bodies{
        Send(*_panel, "GET", "/api/panel/me", nullptr, &owner).Body,
        Send(*_panel, "GET", "/api/panel/me/two-factor", nullptr, &owner).Body,
        Send(*_panel, "GET", "/api/panel/session", nullptr, &owner).Body,
        used.Body,
        used.Head,
    };
    for (std::string const& spelling : spellings)
        for (std::string const& body : bodies)
            EXPECT_EQ(body.find(spelling), std::string::npos) << spelling << " appeared in " << body;

    std::string error;
    std::optional<PanelStore::Statement> audit = _panel->Store().Prepare(
        "SELECT name, COALESCE(actor_name, ''), COALESCE(address, ''), COALESCE(user_agent, ''), COALESCE(node, ''), COALESCE(error, ''), COALESCE(reason, ''), properties FROM audit_event", error);
    ASSERT_TRUE(audit.has_value()) << error;
    while (audit->Step(error))
        for (int column = 0; column < 8; ++column)
            for (std::string const& spelling : spellings)
                EXPECT_EQ(audit->Text(column).find(spelling), std::string::npos) << spelling << " is in an audit row";
    audit.reset();
    std::optional<PanelStore::Statement> subjects = _panel->Store().Prepare("SELECT kind, COALESCE(subject_id, ''), COALESCE(name, '') FROM audit_subject", error);
    ASSERT_TRUE(subjects.has_value()) << error;
    while (subjects->Step(error))
        for (int column = 0; column < 3; ++column)
            for (std::string const& spelling : spellings)
                EXPECT_EQ(subjects->Text(column).find(spelling), std::string::npos);
    subjects.reset();
    for (std::string const& line : _harness.Store().Texts("Capture"))
        for (std::string const& spelling : spellings)
            EXPECT_EQ(line.find(spelling), std::string::npos) << spelling << " is in a log line";

    std::set<std::string> expected;
    std::set<std::string> plainHashes;
    for (std::string const& code : normalized)
    {
        std::optional<std::string> const hash = _panel->TwoFactor().RecoveryHash(_panel->Keyring().ActiveId(), owner.UserId, code);
        ASSERT_TRUE(hash.has_value());
        expected.insert(*hash);
        plainHashes.insert(Base64::Encode(SHA256::GetDigestOf(code), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted));
        plainHashes.insert(Base64::Encode(SHA256::GetDigestOf(code)));
    }
    std::optional<PanelStore::Statement> stored = _panel->Store().Prepare("SELECT code_hash, key_id FROM panel_recovery_code WHERE user_id = ?", error);
    ASSERT_TRUE(stored.has_value()) << error;
    stored->Bind(1, owner.UserId);
    std::set<std::string> held;
    while (stored->Step(error))
    {
        held.insert(stored->Text(0));
        EXPECT_EQ(stored->Int64(1), _panel->Keyring().ActiveId());
        EXPECT_FALSE(plainHashes.contains(stored->Text(0))) << "a plain SHA-256 of the code is never what is kept";
        EXPECT_EQ(std::find(normalized.begin(), normalized.end(), stored->Text(0)), normalized.end());
    }
    stored.reset();
    EXPECT_EQ(held, expected) << "each row is the keyring's HMAC-SHA-256 of the operator's id and the code";

    std::filesystem::path const file = _panel->Store().GetFile();
    std::string const keyring = ReadAll(_panel->Keyring().GetFile());
    std::string const database = ReadAll(file);
    std::string const log = ReadAll(std::filesystem::path(file).concat("-wal"));
    ASSERT_FALSE(database.empty());
    for (std::string const& spelling : spellings)
    {
        EXPECT_EQ(database.find(spelling), std::string::npos) << spelling << " is in the store";
        EXPECT_EQ(log.find(spelling), std::string::npos) << spelling << " is in the store's write-ahead log";
        EXPECT_EQ(keyring.find(spelling), std::string::npos);
    }
}

TEST_F(PanelTwoFactorTest, TheOneTimePasswordLinkDoesNotSignInPastTheSecondFactor)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    std::string const token = _panel->MintPasswordLink(owner.UserId);

    AdminClientResponse const reset = Send(*_panel, "POST", "/api/panel/reset", { { "token", token }, { "password", "a brand new long password" } });
    ASSERT_EQ(reset.Status, 200) << reset.Body;
    EXPECT_TRUE(Json(reset).value("second_factor", false));
    EXPECT_TRUE(SessionCookie(reset).empty()) << "the link sets the password and still asks for a code";
    Browser linked;
    linked.Challenge = ChallengeCookie(reset);
    ASSERT_FALSE(linked.Challenge.empty());
    _now += Totp::StepSeconds;
    AdminClientResponse const opened = SecondFactor(*_panel, linked, { { "code", CodeAt(enrolled->Secret, _now) } });
    ASSERT_EQ(opened.Status, 200) << opened.Body;
    EXPECT_NE(ReasonOfLast("panel:session.opened").find("one-time password link"), std::string::npos);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &linked).Status, 200);
}

TEST_F(PanelTwoFactorTest, WrongCodesAreHeldBackInTheirOwnBucket)
{
    Start("Panel.TwoFactorFailureLimit = 3\nPanel.TwoFactorChallengeAttempts = 20\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += Totp::StepSeconds;

    Browser guesser;
    ASSERT_EQ(SignIn(*_panel, guesser, "owner").Status, 200);
    std::string const wrong = WrongCode(enrolled->Secret, _now);
    for (int attempt = 0; attempt < 3; ++attempt)
        EXPECT_EQ(SecondFactor(*_panel, guesser, { { "code", wrong } }).Status, 401) << attempt;
    AdminClientResponse const held = SecondFactor(*_panel, guesser, { { "code", CodeAt(enrolled->Secret, _now) } });
    EXPECT_EQ(held.Status, 429) << held.Body;
    EXPECT_NE(held.Head.find("Retry-After:"), std::string::npos);
    EXPECT_EQ(CountResult("panel:session.second_factor_throttled", AuditResult::Throttled), 1);
    EXPECT_TRUE(_panel->SignInThrottle().Check("owner", "127.0.0.1").Allowed) << "wrong codes do not spend the password's own count";

    _panel->SecondFactorThrottle().Clear();
    EXPECT_EQ(SecondFactor(*_panel, guesser, { { "code", CodeAt(enrolled->Secret, _now) } }).Status, 200);
}

TEST_F(PanelTwoFactorTest, AWindowOfNoneTakesOnlyTheCurrentStep)
{
    Start("Panel.TwoFactorWindow = 0\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += 2 * static_cast<int64>(Totp::StepSeconds);

    Browser late;
    ASSERT_EQ(SignIn(*_panel, late, "owner").Status, 200);
    EXPECT_EQ(SecondFactor(*_panel, late, { { "code", CodeAt(enrolled->Secret, _now - Totp::StepSeconds) } }).Status, 401) << "the step before now is outside a window of none";
    EXPECT_EQ(SecondFactor(*_panel, late, { { "code", CodeAt(enrolled->Secret, _now) } }).Status, 200);
}

TEST_F(PanelTwoFactorTest, SetupKeepsAPendingSecretUntilItIsReplaced)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    AdminClientResponse const first = Send(*_panel, "POST", "/api/panel/me/two-factor/setup", nlohmann::json::object(), &owner);
    ASSERT_EQ(first.Status, 200) << first.Body;
    EXPECT_TRUE(Json(first)["fresh"].get<bool>());
    EXPECT_EQ(Json(first)["uri"].get<std::string>().rfind("otpauth://totp/", 0), 0u);
    AdminClientResponse const again = Send(*_panel, "POST", "/api/panel/me/two-factor/setup", nlohmann::json::object(), &owner);
    EXPECT_EQ(SecretOf(again), SecretOf(first)) << "a secret already scanned is not swapped silently";
    EXPECT_FALSE(Json(again)["fresh"].get<bool>());
    AdminClientResponse const replaced = Send(*_panel, "POST", "/api/panel/me/two-factor/setup", { { "replace", true } }, &owner);
    ASSERT_EQ(replaced.Status, 200);
    EXPECT_NE(SecretOf(replaced), SecretOf(first));

    AdminClientResponse const stale = Send(*_panel, "POST", "/api/panel/me/two-factor/enable", { { "password", Password }, { "code", CodeAt(SecretOf(first), _now) } }, &owner);
    EXPECT_EQ(stale.Status, 403) << "only the pending secret turns it on";
    AdminClientResponse const wrongPassword = Send(*_panel, "POST", "/api/panel/me/two-factor/enable", { { "password", "not the password" }, { "code", CodeAt(SecretOf(replaced), _now) } }, &owner);
    EXPECT_EQ(wrongPassword.Status, 403) << "enabling needs the password";
    AdminClientResponse const enabled = Send(*_panel, "POST", "/api/panel/me/two-factor/enable", { { "password", Password }, { "code", CodeAt(SecretOf(replaced), _now) } }, &owner);
    EXPECT_EQ(enabled.Status, 200) << enabled.Body;
    EXPECT_EQ(CountResult("user:two_factor.enabled", AuditResult::Refused), 2);
    EXPECT_EQ(CountResult("user:two_factor.enabled", AuditResult::Succeeded), 1);
}

TEST_F(PanelTwoFactorTest, MovingToAnotherAppTakesACodeFromTheAppInUse)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _now);
    ASSERT_TRUE(enrolled.has_value());
    _now += Totp::StepSeconds;
    Browser other;
    ASSERT_EQ(SignIn(*_panel, other, "owner").Status, 200);
    std::string const signedInWith = CodeAt(enrolled->Secret, _now);
    ASSERT_EQ(SecondFactor(*_panel, other, { { "code", signedInWith } }).Status, 200);
    _now += Totp::StepSeconds;

    AdminClientResponse const setup = Send(*_panel, "POST", "/api/panel/me/two-factor/setup", { { "replace", true } }, &owner);
    ASSERT_EQ(setup.Status, 200) << setup.Body;
    std::vector<uint8> const moved = SecretOf(setup);
    ASSERT_FALSE(moved.empty());
    std::string const fresh = CodeAt(moved, _now);
    auto const enable = [&](nlohmann::json const& current)
    {
        nlohmann::json body{ { "password", Password }, { "code", fresh } };
        body.update(current);
        return Send(*_panel, "POST", "/api/panel/me/two-factor/enable", body, &owner);
    };

    EXPECT_EQ(enable(nlohmann::json::object()).Status, 422) << "a session and the password alone cannot swap the factor out";
    EXPECT_EQ(enable({ { "current_code", WrongCode(enrolled->Secret, _now) } }).Status, 403);
    EXPECT_EQ(enable({ { "current_code", signedInWith } }).Status, 403) << "a code already accepted is not a current one";
    EXPECT_EQ(ReasonOfLast("user:two_factor.enabled"), "replayed");
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &other).Status, 200) << "nothing refused ended anything";
    EXPECT_EQ(CountResult("user:two_factor.enabled", AuditResult::Refused), 2);

    AdminClientResponse const swapped = enable({ { "current_code", CodeAt(enrolled->Secret, _now) } });
    ASSERT_EQ(swapped.Status, 200) << "a code from each app in the same step moves it: " << swapped.Body;
    nlohmann::json const answer = Json(swapped);
    ASSERT_TRUE(answer.contains("recovery_codes") && answer["recovery_codes"].is_array());
    ASSERT_EQ(answer["recovery_codes"].size(), RecoveryCode::Count);
    EXPECT_NE(ReasonOfLast("user:two_factor.enabled").find("moved to another authenticator app"), std::string::npos);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &other).Status, 401) << "every other session has ended";
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &owner).Status, 200);

    _now += Totp::StepSeconds;
    Browser after;
    ASSERT_EQ(SignIn(*_panel, after, "owner").Status, 200);
    EXPECT_EQ(SecondFactor(*_panel, after, { { "code", CodeAt(enrolled->Secret, _now) } }).Status, 401) << "the old app signs nobody in";
    EXPECT_EQ(SecondFactor(*_panel, after, { { "recovery_code", enrolled->Codes[0] } }).Status, 401) << "nor do the old recovery codes";
    ASSERT_EQ(SecondFactor(*_panel, after, { { "code", CodeAt(moved, _now) } }).Status, 200);

    _now += Totp::StepSeconds;
    AdminClientResponse const again = Send(*_panel, "POST", "/api/panel/me/two-factor/setup", { { "replace", true } }, &after);
    ASSERT_EQ(again.Status, 200) << again.Body;
    nlohmann::json const byRecovery{ { "password", Password }, { "code", CodeAt(SecretOf(again), _now) }, { "current_recovery_code", answer["recovery_codes"][0] } };
    AdminClientResponse const movedAgain = Send(*_panel, "POST", "/api/panel/me/two-factor/enable", byRecovery, &after);
    EXPECT_EQ(movedAgain.Status, 200) << "a recovery code stands in for the app in use: " << movedAgain.Body;
}

TEST_F(PanelTwoFactorTest, TheConsoleResetTurnsItOffAndEndsEverySession)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_TRUE(Enroll(*_panel, owner, _now).has_value());
    std::string error;
    std::optional<PanelUser> const user = _panel->Users().FindById(owner.UserId, error);
    ASSERT_TRUE(user.has_value()) << error;
    ASSERT_TRUE(user->TwoFactor);
    ASSERT_TRUE(_panel->ResetTwoFactor(*user, "console", error)) << error;

    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &owner).Status, 401);
    std::optional<PanelStore::Statement> row = _panel->Store().Prepare("SELECT actor_type, actor_name FROM audit_event WHERE name = 'panel:user.two_factor_reset'", error);
    ASSERT_TRUE(row.has_value()) << error;
    ASSERT_TRUE(row->Step(error)) << error;
    EXPECT_EQ(row->Text(0), "system");
    EXPECT_EQ(row->Text(1), "console");
    row.reset();
    Browser back;
    AdminClientResponse const signedIn = SignIn(*_panel, back, "owner");
    ASSERT_EQ(signedIn.Status, 200);
    EXPECT_FALSE(Json(signedIn).value("second_factor", false));
}
