/*
 * Project Ambrose by Imjustchico
 * Tests that panel settings keep secrets out of answers, logs and audit rows, environment-owned values stay locked, invalid values are refused, and only users holding panel.settings can read or change any settings group. The mail test reaches only the signed-in user through a fake SMTP server and shows that server's error on a bad password, and with the captcha on and its provider unreachable, sign-in after repeated failures is refused with a clear error.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelPermissions.h"
#include "SourceFolder.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
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

namespace
{
    class CaptchaVerifyOverride
    {
    public:
        explicit CaptchaVerifyOverride(std::string_view url)
        {
#ifdef _WIN32
            _putenv_s("AMBROSE_TEST_CAPTCHA_VERIFY_URL", std::string(url).c_str());
#else
            setenv("AMBROSE_TEST_CAPTCHA_VERIFY_URL", std::string(url).c_str(), 1);
#endif
        }

        ~CaptchaVerifyOverride()
        {
#ifdef _WIN32
            _putenv_s("AMBROSE_TEST_CAPTCHA_VERIFY_URL", "");
#else
            unsetenv("AMBROSE_TEST_CAPTCHA_VERIFY_URL");
#endif
        }

        CaptchaVerifyOverride(CaptchaVerifyOverride const&) = delete;
        CaptchaVerifyOverride& operator=(CaptchaVerifyOverride const&) = delete;
    };

    class FakeSmtpServer
    {
    public:
        explicit FakeSmtpServer(bool rejectAuth) : _rejectAuth(rejectAuth),
            _acceptor(_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0))
        {
            _port = _acceptor.local_endpoint().port();
            _thread = std::thread([this] { Serve(); });
        }

        ~FakeSmtpServer()
        {
            asio::error_code ignored;
            {
                asio::ip::tcp::socket wake(_context);
                wake.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), _port), ignored);
            }
            if (_thread.joinable())
                _thread.join();
        }

        FakeSmtpServer(FakeSmtpServer const&) = delete;
        FakeSmtpServer& operator=(FakeSmtpServer const&) = delete;

        uint16 Port() const
        {
            return _port;
        }

        std::vector<std::string> Recipients() const
        {
            std::lock_guard const lock(_mutex);
            return _recipients;
        }

    private:
        void Serve()
        {
            asio::ip::tcp::socket socket(_context);
            asio::error_code error;
            _acceptor.accept(socket, error);
            if (!error)
                Handle(socket);
        }

        std::optional<std::string> ReadLine(asio::ip::tcp::socket& socket)
        {
            asio::error_code error;
            asio::read_until(socket, _readBuffer, "\n", error);
            if (error)
                return std::nullopt;
            std::istream stream(&_readBuffer);
            std::string line;
            std::getline(stream, line);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            return line;
        }

        static void WriteLine(asio::ip::tcp::socket& socket, std::string_view line)
        {
            std::string const framed = std::string(line) + "\r\n";
            asio::error_code error;
            asio::write(socket, asio::buffer(framed), error);
        }

        void Handle(asio::ip::tcp::socket& socket)
        {
            WriteLine(socket, "220 fake.test ESMTP");
            for (;;)
            {
                std::optional<std::string> const read = ReadLine(socket);
                if (!read)
                    return;
                std::string const& line = *read;
                if (line.starts_with("EHLO") || line.starts_with("HELO"))
                {
                    WriteLine(socket, "250-fake.test");
                    WriteLine(socket, "250 AUTH LOGIN");
                }
                else if (line == "AUTH LOGIN")
                {
                    WriteLine(socket, "334 VXNlcm5hbWU6");
                    if (!ReadLine(socket))
                        return;
                    WriteLine(socket, "334 UGFzc3dvcmQ6");
                    if (!ReadLine(socket))
                        return;
                    if (_rejectAuth)
                    {
                        WriteLine(socket, "535 5.7.8 Authentication credentials invalid");
                        return;
                    }
                    WriteLine(socket, "235 2.7.0 Authentication successful");
                }
                else if (line.starts_with("MAIL FROM:"))
                    WriteLine(socket, "250 OK");
                else if (line.starts_with("RCPT TO:"))
                {
                    std::string address = std::string(line.substr(8));
                    while (!address.empty() && (address.front() == '<' || address.front() == ' '))
                        address.erase(0, 1);
                    while (!address.empty() && (address.back() == '>' || address.back() == ' '))
                        address.pop_back();
                    std::lock_guard const lock(_mutex);
                    _recipients.push_back(address);
                    WriteLine(socket, "250 OK");
                }
                else if (line == "DATA")
                {
                    WriteLine(socket, "354 End data with <CR><LF>.<CR><LF>");
                    for (;;)
                    {
                        std::optional<std::string> const data = ReadLine(socket);
                        if (!data)
                            return;
                        if (*data == ".")
                            break;
                    }
                    WriteLine(socket, "250 OK");
                }
                else if (line == "QUIT")
                {
                    WriteLine(socket, "221 Bye");
                    return;
                }
                else if (line == "RSET")
                    WriteLine(socket, "250 OK");
                else
                    WriteLine(socket, "502 Unimplemented");
            }
        }

        bool _rejectAuth;
        asio::streambuf _readBuffer;
        asio::io_context _context;
        asio::ip::tcp::acceptor _acceptor;
        uint16 _port = 0;
        std::thread _thread;
        mutable std::mutex _mutex;
        std::vector<std::string> _recipients;
    };
}

TEST_F(PanelSettingsTest, MailTestReachesOnlyTheSignedInUser)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string error;
    std::optional<PanelStore::Statement> setEmail = _panel->Store().Prepare("UPDATE panel_user SET email = ? WHERE id = ?", error);
    ASSERT_TRUE(setEmail.has_value()) << error;
    setEmail->Bind(1, "owner@example.test");
    setEmail->Bind(2, owner.UserId);
    ASSERT_TRUE(setEmail->Run(error)) << error;

    FakeSmtpServer smtp(false);
    ASSERT_TRUE(_panel->Settings().Update({
        { "Mail.SmtpHost", "127.0.0.1" },
        { "Mail.SmtpPort", std::to_string(smtp.Port()) },
        { "Mail.TlsMode", "none" },
        { "Mail.FromAddress", "panel@example.test" },
    }, owner.UserId, error)) << error;

    AdminClientResponse const answer = Send("POST", "/api/panel/settings/mail/test",
        { { "to", "attacker@evil.example" } }, &owner);
    EXPECT_EQ(answer.Status, 200) << answer.Body;

    std::vector<std::string> const recipients = smtp.Recipients();
    ASSERT_EQ(recipients.size(), 1u);
    EXPECT_EQ(recipients[0], "owner@example.test");
}

TEST_F(PanelSettingsTest, MailTestShowsTheSmtpServersErrorOnABadPassword)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string error;
    std::optional<PanelStore::Statement> setEmail = _panel->Store().Prepare("UPDATE panel_user SET email = ? WHERE id = ?", error);
    ASSERT_TRUE(setEmail.has_value()) << error;
    setEmail->Bind(1, "owner@example.test");
    setEmail->Bind(2, owner.UserId);
    ASSERT_TRUE(setEmail->Run(error)) << error;

    FakeSmtpServer smtp(true);
    ASSERT_TRUE(_panel->Settings().Update({
        { "Mail.SmtpHost", "127.0.0.1" },
        { "Mail.SmtpPort", std::to_string(smtp.Port()) },
        { "Mail.TlsMode", "none" },
        { "Mail.Username", "paneluser" },
        { "Mail.Password", "wrong-password" },
        { "Mail.FromAddress", "panel@example.test" },
    }, owner.UserId, error)) << error;

    AdminClientResponse const answer = Send("POST", "/api/panel/settings/mail/test", {}, &owner);
    EXPECT_EQ(answer.Status, 502) << answer.Body;
    EXPECT_NE(answer.Body.find("535"), std::string::npos) << answer.Body;
    EXPECT_NE(answer.Body.find("Authentication credentials invalid"), std::string::npos) << answer.Body;
}

TEST_F(PanelSettingsTest, CaptchaUnreachableRefusesSignInWithAClearError)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string error;
    ASSERT_TRUE(_panel->Settings().Update({
        { "Security.CaptchaProvider", "recaptcha" },
        { "Security.CaptchaSiteKey", "test-site-key" },
        { "Security.CaptchaSecret", "test-secret" },
    }, owner.UserId, error)) << error;

    AdminClientResponse const clean = Send("POST", "/api/panel/session",
        { { "username", "owner" }, { "password", "a good long password" } });
    EXPECT_EQ(clean.Status, 200) << clean.Body;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        AdminClientResponse const wrong = Send("POST", "/api/panel/session",
            { { "username", "owner" }, { "password", "not the password" } });
        EXPECT_EQ(wrong.Status, 401) << wrong.Body;
    }

    CaptchaVerifyOverride const unreachable("http://127.0.0.1:9/");
    AdminClientResponse const refused = Send("POST", "/api/panel/session",
        { { "username", "owner" }, { "password", "a good long password" }, { "captcha", "test-token" } });
    EXPECT_EQ(refused.Status, 503) << refused.Body;
    EXPECT_NE(refused.Body.find("captcha"), std::string::npos) << refused.Body;
    EXPECT_NE(refused.Body.find("could not be reached"), std::string::npos) << refused.Body;
}
