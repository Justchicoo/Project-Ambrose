/*
 * Project Ambrose by Imjustchico
 * Tests that panel settings keep secrets out of answers, logs and audit rows, environment-owned values stay locked, invalid values are refused, and only users holding panel.settings can read or change any settings group.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelPermissions.h"
#include "SourceFolder.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class TrustedProxyEnvironment
    {
    public:
        TrustedProxyEnvironment()
        {
#ifdef _WIN32
            char* value = nullptr;
            std::size_t length = 0;
            if (_dupenv_s(&value, &length, "AMBROSE_PANEL_TRUSTED_PROXIES") == 0 && value)
            {
                _previous = value;
                _hadPrevious = true;
                std::free(value);
            }
#else
            if (char const* const value = std::getenv("AMBROSE_PANEL_TRUSTED_PROXIES"))
            {
                _previous = value;
                _hadPrevious = true;
            }
#endif
        }

        ~TrustedProxyEnvironment()
        {
            if (_hadPrevious)
                Set(_previous);
            else
                Clear();
        }

        bool Set(std::string_view value)
        {
#ifdef _WIN32
            return _putenv_s("AMBROSE_PANEL_TRUSTED_PROXIES", std::string(value).c_str()) == 0;
#else
            return setenv("AMBROSE_PANEL_TRUSTED_PROXIES", std::string(value).c_str(), 1) == 0;
#endif
        }

    private:
        void Clear()
        {
#ifdef _WIN32
            _putenv_s("AMBROSE_PANEL_TRUSTED_PROXIES", "");
#else
            unsetenv("AMBROSE_PANEL_TRUSTED_PROXIES");
#endif
        }

        std::string _previous;
        bool _hadPrevious = false;
    };

    struct Credentials
    {
        std::string Cookie;
        std::string Csrf;
        int64 UserId = 0;
    };

    class PanelSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\nPanel.TrustedProxies =\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        AdminClientResponse Send(std::string method, std::string path, nlohmann::json body = {}, Credentials const* credentials = nullptr)
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "");
            AdminClientRequest request{ std::move(method), std::move(path), body.is_null() ? "" : body.dump(), "application/json", "" };
            if (credentials)
            {
                request.Headers.emplace_back("Cookie", credentials->Cookie);
                if (request.Method != "GET")
                {
                    request.Headers.emplace_back("Origin", "http://127.0.0.1:" + std::to_string(_panel->GetPort()));
                    request.Headers.emplace_back("X-CSRF-Token", credentials->Csrf);
                }
            }
            return client.Send(request, std::chrono::seconds(20));
        }

        bool MakeOwner(Credentials& credentials)
        {
            std::string token;
            for (std::string const& line : _harness.Store().Texts("Capture"))
            {
                std::size_t const at = line.find("token=");
                if (at != std::string::npos)
                    token = line.substr(at + 6);
            }
            if (token.empty())
                return false;

            AdminClientResponse const claimed = Send("POST", "/api/panel/claim",
                { { "token", token }, { "username", "owner" }, { "password", "a good long password" } });
            if (claimed.Status != 200)
                return false;
            nlohmann::json const answer = nlohmann::json::parse(claimed.Body, nullptr, false);
            credentials.Cookie = CookieFrom(claimed);
            credentials.Csrf = answer.value("csrf", "");
            credentials.UserId = answer.value("user", nlohmann::json::object()).value("id", int64{ 0 });
            return !credentials.Cookie.empty() && !credentials.Csrf.empty() && credentials.UserId != 0;
        }

        bool MakeOperator(Credentials& credentials)
        {
            Credentials owner;
            if (!MakeOwner(owner))
                return false;
            std::string error;
            int64 operatorId = 0;
            if (_panel->Users().Create("operator", "another good password", false, false, &operatorId, error) != PanelUserResult::Ok)
                return false;
            if (_panel->Users().SetRole(operatorId, PanelRole::Operator, owner.UserId, error) != PanelUserResult::Ok)
                return false;
            AdminClientResponse const signedIn = Send("POST", "/api/panel/session",
                { { "username", "operator" }, { "password", "another good password" } });
            if (signedIn.Status != 200)
                return false;
            nlohmann::json const answer = nlohmann::json::parse(signedIn.Body, nullptr, false);
            credentials.Cookie = CookieFrom(signedIn);
            credentials.Csrf = answer.value("csrf", "");
            credentials.UserId = operatorId;
            return !credentials.Cookie.empty() && !credentials.Csrf.empty();
        }

        std::string CookieFrom(AdminClientResponse const& response) const
        {
            std::size_t const at = response.Head.find("Set-Cookie: ");
            if (at == std::string::npos)
                return {};
            std::string const rest = response.Head.substr(at + 12);
            return rest.substr(0, rest.find(';'));
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
    };
}

TEST_F(PanelSettingsTest, KeepsSavedSecretsOutOfAnswersLogsAndAuditRows)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string const secret = "panel-secret-never-return";

    AdminClientResponse const changed = Send("PATCH", "/api/panel/settings",
        { { "values", { { "Mail.Password", secret } } } }, &owner);
    ASSERT_EQ(changed.Status, 200);
    EXPECT_EQ(changed.Body.find(secret), std::string::npos);
    nlohmann::json const answer = nlohmann::json::parse(changed.Body);
    auto const password = std::find_if(answer["settings"].begin(), answer["settings"].end(),
        [](nlohmann::json const& setting) { return setting["key"] == "Mail.Password"; });
    ASSERT_NE(password, answer["settings"].end());
    EXPECT_EQ((*password)["value"], "***");

    AdminClientResponse const unchanged = Send("PATCH", "/api/panel/settings",
        { { "values", { { "Mail.Password", "***" } } } }, &owner);
    ASSERT_EQ(unchanged.Status, 200) << unchanged.Body;
    EXPECT_EQ(unchanged.Body.find(secret), std::string::npos);

    std::string error;
    std::optional<PanelStore::Statement> saved = _panel->Store().Prepare("SELECT value FROM panel_setting WHERE key = 'Mail.Password'", error);
    ASSERT_TRUE(saved.has_value()) << error;
    ASSERT_TRUE(saved->Step(error)) << error;
    EXPECT_TRUE(saved->Text(0) == secret);

    std::optional<PanelStore::Statement> audit = _panel->Store().Prepare(
        "SELECT name, actor_name, address, user_agent, node, error, reason, properties FROM audit_event", error);
    ASSERT_TRUE(audit.has_value()) << error;
    bool sawSettingsChange = false;
    while (audit->Step(error))
    {
        sawSettingsChange = sawSettingsChange || audit->Text(0) == "panel:settings.changed";
        for (int column = 0; column < 8; ++column)
            EXPECT_EQ(audit->Text(column).find(secret), std::string::npos);
    }
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_TRUE(sawSettingsChange);
    for (std::string const& line : _harness.Store().Texts("Capture"))
        EXPECT_EQ(line.find(secret), std::string::npos);
}

TEST_F(PanelSettingsTest, KeepsTrustedProxiesLockedToTheEnvironmentLayer)
{
    TrustedProxyEnvironment environment;
    ASSERT_TRUE(environment.Set("198.51.100.7"));
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));

    AdminClientResponse const answer = Send("GET", "/api/panel/settings?group=security", {}, &owner);
    ASSERT_EQ(answer.Status, 200) << answer.Body;
    nlohmann::json const body = nlohmann::json::parse(answer.Body);
    auto const trustedProxies = std::find_if(body["settings"].begin(), body["settings"].end(),
        [](nlohmann::json const& setting) { return setting["key"] == "Panel.TrustedProxies"; });
    ASSERT_NE(trustedProxies, body["settings"].end());
    EXPECT_EQ((*trustedProxies)["value"], "198.51.100.7");
    EXPECT_TRUE((*trustedProxies)["locked"].get<bool>());
    EXPECT_EQ((*trustedProxies)["layer"], "environment");

    AdminClientResponse const changed = Send("PATCH", "/api/panel/settings",
        { { "values", { { "Panel.TrustedProxies", "203.0.113.9" } } } }, &owner);
    EXPECT_EQ(changed.Status, 409) << changed.Body;
    EXPECT_NE(changed.Body.find("locked by environment"), std::string::npos) << changed.Body;
}

TEST_F(PanelSettingsTest, RefusesValuesWithTheWrongTypeOrOutsideTheirBounds)
{
    std::string error;
    EXPECT_FALSE(_panel->Settings().Update({ { "Security.SignInBurst", "0" } }, 1, error));
    EXPECT_NE(error.find("outside its allowed range"), std::string::npos);
    EXPECT_FALSE(_panel->Settings().Update({ { "Security.SignInBurst", 10 } }, 1, error));
    EXPECT_NE(error.find("unknown or invalid setting"), std::string::npos);
}

TEST_F(PanelSettingsTest, RequiresPanelSettingsForEveryGroupAndBothMethods)
{
    Credentials operatorUser;
    ASSERT_TRUE(MakeOperator(operatorUser));
    nlohmann::json const signedIn = nlohmann::json::parse(Send("GET", "/api/panel/me", {}, &operatorUser).Body);
    std::vector<std::string> const permissions = signedIn["permissions"].get<std::vector<std::string>>();
    ASSERT_NE(std::find(permissions.begin(), permissions.end(), "settings.read"), permissions.end());
    ASSERT_EQ(std::find(permissions.begin(), permissions.end(), "panel.settings"), permissions.end());

    std::vector<std::pair<std::string, std::string>> const groups{
        { "general", "Panel.Name" },
        { "mail", "Mail.SmtpHost" },
        { "security", "Security.SignInBurst" },
    };
    for (auto const& [group, key] : groups)
    {
        EXPECT_EQ(Send("GET", "/api/panel/settings?group=" + group, {}, &operatorUser).Status, 403) << group;
        EXPECT_EQ(Send("PATCH", "/api/panel/settings?group=" + group,
            { { "values", { { key, "changed" } } } }, &operatorUser).Status, 403) << group;
    }
}
