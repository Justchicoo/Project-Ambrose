/*
 * Project Ambrose by Imjustchico
 * Tests the single-use sign-in links against a real panel listener and the supervisor's admin API, with the links' clock moved by the test and a trusted proxy standing in for a far peer: a local link signs in once, only from loopback and only within its sixty seconds, and is burned when a far peer tries it; the admin route needs its token and on an empty panel makes the owner with no password anywhere; an owner claim or password link printed before a restart works once after it; a pairing line pins the certificate the listener serves, names only an address the panel answers for and is refused for plain HTTP beyond loopback; twenty wrong or spent tokens from one address are held back while another address is not, and a disabled operator's link is refused; a link never skips a second factor, the issued row and the session row name the link, the command line prints only the link from the token file without writing one, and no token reaches a log line, an audit row, the command record, the store or the keyring, which keep only its hash.
 */

#include "AdminClient.h"
#include "AdminCommand.h"
#include "AdminServer.h"
#include "ConfigMgr.h"
#include "ConsoleCommandTable.h"
#include "Environment.h"
#include "IpAddress.h"
#include "ListenerSettings.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelCommands.h"
#include "PanelLinkClient.h"
#include "PanelLinks.h"
#include "PanelTestSession.h"
#include "TlsCertificate.h"
#include "Totp.h"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace PanelTest;

namespace
{
    constexpr char const* Bearer = "0123456789abcdef0123456789abcdef";

    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    std::string ReadAll(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    std::set<std::string> RunsOfTokenLength(std::string_view text)
    {
        std::set<std::string> runs;
        auto const urlSafe = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; };
        std::size_t start = 0;
        for (std::size_t index = 0; index <= text.size(); ++index)
        {
            if (index < text.size() && urlSafe(text[index]))
                continue;
            if (index - start == PanelLinks::TokenLength)
                runs.emplace(text.substr(start, index - start));
            start = index + 1;
        }
        return runs;
    }

    std::set<std::string> KeysOf(nlohmann::json const& body)
    {
        std::set<std::string> keys;
        if (body.is_object())
            for (auto const& [key, value] : body.items())
                keys.insert(key);
        return keys;
    }

    std::optional<std::string> SourceTowards(asio::ip::address const& bound)
    {
        asio::io_context context;
        asio::ip::udp::socket socket(context);
        std::error_code code;
        asio::ip::address const remote = bound.is_v6() ? asio::ip::make_address("2001:db8::1", code) : asio::ip::make_address("192.0.2.1", code);
        if (code)
            return std::nullopt;
        socket.open(remote.is_v6() ? asio::ip::udp::v6() : asio::ip::udp::v4(), code);
        if (code)
            return std::nullopt;
        socket.connect(asio::ip::udp::endpoint(remote, 9), code);
        if (code)
            return std::nullopt;
        asio::ip::udp::endpoint const local = socket.local_endpoint(code);
        if (code)
            return std::nullopt;
        return local.address().to_string();
    }

    struct Transcript
    {
        std::vector<std::string> Lines = {};

        ConsoleCommandTable::Reply Reply()
        {
            return [this](std::string_view text) { Lines.emplace_back(text); };
        }
    };

    class PanelLinkTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
        }

        void TearDown() override
        {
            StopBoth();
        }

        void StopBoth()
        {
            if (_admin)
                _admin->Stop();
            _admin.reset();
            if (_panel)
                _panel->Stop();
            _panel.reset();
        }

        std::string Secure()
        {
            std::filesystem::path const certificate = _directory.Path() / "panel.crt";
            std::filesystem::path const key = _directory.Path() / "panel.key";
            std::string error;
            if (!std::filesystem::exists(certificate))
            {
                EXPECT_TRUE(TlsCertificate::CreateSelfSigned(certificate, key, "Ambrose panel", { "localhost", "127.0.0.1", "::1" }, 60, error)) << error;
            }
            return fmt::format("Panel.CertificateFile = \"{}\"\nPanel.PrivateKeyFile = \"{}\"\n", certificate.generic_string(), key.generic_string());
        }

        void Start(std::string const& extra = "")
        {
            _extra = extra;
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\nPanel.TrustedProxies = 127.0.0.1\n" + extra)).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            _panel->Links().SetClock([this] { return _now; });
            _panel->TwoFactor().SetClock([this] { return _seconds; });
            if (_prepare)
                _prepare(*_panel);
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;

            _admin = std::make_unique<AdminServer>(_harness.GetLog(), "supervisor", _directory.Path() / "data", _directory.Path());
            _panel->RegisterAdminRoutes(_admin->Routes());
            ListenerSettings settings;
            settings.Enable = true;
            settings.BindIp = "127.0.0.1";
            settings.Port = 0;
            settings.Token = Bearer;
            ASSERT_TRUE(_admin->Start(settings, error)) << error;
        }

        void Restart()
        {
            StopBoth();
            Start(_extra);
        }

        AdminClientResponse Mint(nlohmann::json const& body, std::string const& bearer = Bearer)
        {
            AdminClient const client("127.0.0.1", _admin->GetPort(), bearer);
            AdminClientRequest const request{ "POST", std::string(Panel::LinksPath), body.dump(), "application/json", "" };
            return client.Send(request, std::chrono::seconds(30));
        }

        std::string MintToken(nlohmann::json const& body)
        {
            AdminClientResponse const minted = Mint(body);
            EXPECT_EQ(minted.Status, 200) << minted.Body;
            nlohmann::json const answer = Json(minted);
            return answer.is_object() ? answer.value("token", std::string()) : std::string();
        }

        AdminClientResponse Claim(std::string const& token, std::string const& username = "owner")
        {
            return Send(*_panel, "POST", "/api/panel/claim", { { "token", token }, { "username", username }, { "password", std::string(Password) } });
        }

        nlohmann::json PropertiesOfLast(std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT properties FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            if (!rows)
                return nullptr;
            rows->Bind(1, name);
            if (!rows->Step(error))
                return nullptr;
            return nlohmann::json::parse(rows->Text(0), nullptr, false);
        }

        std::string ColumnOfLast(std::string const& column, std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare(fmt::format("SELECT COALESCE({}, '') FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", column), error);
            if (!rows)
                return {};
            rows->Bind(1, name);
            return rows->Step(error) ? rows->Text(0) : std::string();
        }

        std::optional<PanelLink> LinkOf(std::string const& token)
        {
            std::string error;
            std::string const id = PanelLinks::HashOf(token).substr(0, PanelLinks::IdLength);
            for (PanelLink const& link : _panel->Links().List(error))
                if (link.Id == id)
                    return link;
            return std::nullopt;
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
        std::unique_ptr<AdminServer> _admin;
        std::function<void(Panel&)> _prepare;
        std::string _extra;
        int64 _now = PanelStore::NowEpochMs();
        int64 _seconds = 1800000000;
    };
}

TEST_F(PanelLinkTest, ALocalLinkSignsInOnceAndOnlyFromLoopback)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    std::string const first = MintToken({ { "kind", "local" } });
    ASSERT_EQ(first.size(), PanelLinks::TokenLength);
    Browser opened;
    AdminClientResponse const traded = LinkIn(*_panel, opened, first);
    ASSERT_EQ(traded.Status, 200) << traded.Body;
    ASSERT_FALSE(opened.Cookie.empty());
    AdminClientResponse const me = Send(*_panel, "GET", "/api/panel/me", nullptr, &opened);
    ASSERT_EQ(me.Status, 200) << me.Body;
    EXPECT_EQ(Json(me).value("username", std::string()), "owner");
    Browser again;
    AdminClientResponse const twice = LinkIn(*_panel, again, first);
    EXPECT_EQ(twice.Status, 410) << twice.Body;
    EXPECT_EQ(ErrorOf(twice), "link_expired");

    std::string const stranger = MintToken({ { "kind", "local" } });
    Browser elsewhere;
    AdminClientResponse const refused = LinkIn(*_panel, elsewhere, stranger, "203.0.113.9");
    EXPECT_EQ(refused.Status, 403) << refused.Body;
    EXPECT_EQ(ErrorOf(refused), "not_this_machine");
    EXPECT_TRUE(elsewhere.Cookie.empty());
    Browser afterwards;
    AdminClientResponse const burned = LinkIn(*_panel, afterwards, stranger);
    EXPECT_EQ(burned.Status, 410) << "a local link a far peer tried is burned: " << burned.Body;

    std::string const inside = MintToken({ { "kind", "local" } });
    _now += 59999;
    Browser justInTime;
    EXPECT_EQ(LinkIn(*_panel, justInTime, inside).Status, 200);

    std::string const late = MintToken({ { "kind", "local" } });
    _now += 60000;
    Browser tooLate;
    AdminClientResponse const expired = LinkIn(*_panel, tooLate, late);
    EXPECT_EQ(expired.Status, 410) << expired.Body;
    EXPECT_EQ(ErrorOf(expired), "link_expired");
    EXPECT_TRUE(tooLate.Cookie.empty());
}

TEST_F(PanelLinkTest, TheRouteNeedsTheAdminTokenAndOnAnEmptyPanelMakesTheOwnerWithNoPasswordAnywhere)
{
    Start();
    std::string const claim = ClaimToken(_harness);
    ASSERT_EQ(claim.size(), PanelLinks::TokenLength);

    EXPECT_EQ(Mint({ { "kind", "local" }, { "username", "desk_owner" } }, "").Status, 401);
    EXPECT_EQ(Mint({ { "kind", "local" }, { "username", "desk_owner" } }, "fedcba9876543210fedcba9876543210").Status, 401);
    std::string error;
    EXPECT_TRUE(_panel->Users().IsEmpty(error)) << "a refused request made nobody";

    AdminClientResponse const minted = Mint({ { "kind", "local" }, { "username", "desk_owner" } });
    ASSERT_EQ(minted.Status, 200) << minted.Body;
    nlohmann::json const answer = Json(minted);
    EXPECT_EQ(KeysOf(answer), (std::set<std::string>{ "kind", "link", "token", "user_id", "username", "created_owner", "expires_epoch_ms", "expires_seconds" }));
    EXPECT_TRUE(answer.value("created_owner", false));
    EXPECT_EQ(answer.value("username", std::string()), "desk_owner");
    EXPECT_EQ(answer.value("expires_seconds", 0), 60);
    std::string const token = answer.value("token", std::string());
    EXPECT_EQ(answer.value("link", std::string()), fmt::format("http://127.0.0.1:{}/#link?token={}", _panel->GetPort(), token));

    std::optional<PanelUser> const owner = _panel->Users().Find("desk_owner", error);
    ASSERT_TRUE(owner.has_value()) << error;
    EXPECT_EQ(owner->Role, PanelRole::Owner);
    EXPECT_TRUE(owner->MustChange);
    EXPECT_EQ(owner->Id, answer.value("user_id", int64{ 0 }));

    Browser desk;
    AdminClientResponse const traded = LinkIn(*_panel, desk, token);
    ASSERT_EQ(traded.Status, 200) << traded.Body;
    EXPECT_EQ(Json(traded)["user"].value("username", std::string()), "desk_owner");
    AdminClientResponse const claimed = Claim(claim, "second");
    EXPECT_EQ(claimed.Status, 410) << "making the owner spent the owner link: " << claimed.Body;
    EXPECT_FALSE(Json(Send(*_panel, "GET", "/api/panel/session")).value("needs_owner", true));
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:user.created"), 1);

    std::set<std::string> inLog;
    for (std::string const& line : _harness.Store().Texts("Capture"))
        for (std::string const& run : RunsOfTokenLength(line))
            inLog.insert(run);
    EXPECT_TRUE(inLog.empty() || inLog == std::set<std::string>{ claim }) << "the log holds only the owner link it printed at start";
    std::set<std::string> inAudit;
    std::optional<PanelStore::Statement> events = _panel->Store().Prepare(
        "SELECT name, COALESCE(actor_id, ''), COALESCE(actor_name, ''), COALESCE(address, ''), COALESCE(reason, ''), COALESCE(error, ''), properties FROM audit_event", error);
    ASSERT_TRUE(events.has_value()) << error;
    while (events->Step(error))
        for (int column = 0; column < 7; ++column)
            for (std::string const& run : RunsOfTokenLength(events->Text(column)))
                inAudit.insert(run);
    events.reset();
    std::optional<PanelStore::Statement> subjects = _panel->Store().Prepare("SELECT kind, COALESCE(subject_id, ''), COALESCE(name, '') FROM audit_subject", error);
    ASSERT_TRUE(subjects.has_value()) << error;
    while (subjects->Step(error))
        for (int column = 0; column < 3; ++column)
            for (std::string const& run : RunsOfTokenLength(subjects->Text(column)))
                inAudit.insert(run);
    subjects.reset();
    EXPECT_TRUE(inAudit.empty()) << "an audit row holds no token and no password";
    EXPECT_EQ(RunsOfTokenLength(minted.Body), std::set<std::string>{ token }) << "the only secret in the answer is the link's own token";
    EXPECT_EQ(minted.Body.find("password"), std::string::npos);
}

TEST_F(PanelLinkTest, AnOwnerClaimLinkPrintedBeforeARestartClaimsOnceAfterIt)
{
    Start();
    std::string const first = ClaimToken(_harness);
    ASSERT_EQ(first.size(), PanelLinks::TokenLength);

    Restart();
    std::string const second = ClaimToken(_harness);
    ASSERT_EQ(second.size(), PanelLinks::TokenLength);
    ASSERT_NE(first, second) << "each start prints a link of its own";
    EXPECT_TRUE(Json(Send(*_panel, "GET", "/api/panel/session")).value("needs_owner", false));

    AdminClientResponse const claimed = Claim(first);
    ASSERT_EQ(claimed.Status, 200) << claimed.Body;
    EXPECT_EQ(Json(claimed)["user"].value("username", std::string()), "owner");
    EXPECT_EQ(Claim(first, "another").Status, 410);
    AdminClientResponse const later = Claim(second, "another");
    EXPECT_EQ(later.Status, 410) << "making the owner spent every open owner link: " << later.Body;
    EXPECT_FALSE(Json(Send(*_panel, "GET", "/api/panel/session")).value("needs_owner", true));
}

TEST_F(PanelLinkTest, APasswordLinkIssuedBeforeARestartSetsThePasswordOnceAfterIt)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::string const token = _panel->MintPasswordLink(owner.UserId);
    ASSERT_EQ(token.size(), PanelLinks::TokenLength);

    Restart();
    AdminClientResponse const weak = Send(*_panel, "POST", "/api/panel/reset", { { "token", token }, { "password", "short" } });
    EXPECT_EQ(weak.Status, 422) << weak.Body;
    EXPECT_FALSE(Json(weak).contains("token")) << "a refused password leaves the link as it was and hands out no other";
    AdminClientResponse const set = Send(*_panel, "POST", "/api/panel/reset", { { "token", token }, { "password", "a brand new long password" } });
    ASSERT_EQ(set.Status, 200) << set.Body;
    EXPECT_FALSE(SessionCookie(set).empty());
    EXPECT_EQ(Send(*_panel, "POST", "/api/panel/reset", { { "token", token }, { "password", "a third long password" } }).Status, 410);
    Browser signing;
    EXPECT_EQ(SignIn(*_panel, signing, "owner", "a brand new long password").Status, 200);
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:user.password_set"), 1);
}

TEST_F(PanelLinkTest, APairingLinePinsTheCertificateTheListenerServesAndSignsInOnce)
{
    Start(Secure());
    ASSERT_TRUE(_panel->IsSecure());
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    AdminClientResponse const minted = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "127.0.0.1" } });
    ASSERT_EQ(minted.Status, 200) << minted.Body;
    nlohmann::json const answer = Json(minted);
    EXPECT_EQ(KeysOf(answer),
        (std::set<std::string>{ "kind", "line", "token", "host", "port", "fingerprint", "user_id", "username", "created_owner", "expires_epoch_ms", "expires_seconds" }));
    std::string const token = answer.value("token", std::string());
    std::string const fingerprint = answer.value("fingerprint", std::string());
    EXPECT_EQ(answer.value("expires_seconds", 0), 600);
    EXPECT_EQ(answer.value("port", 0), _panel->GetPort());
    EXPECT_EQ(answer.value("line", std::string()), fmt::format("https://127.0.0.1:{}/#link?token={}&sha256={}", _panel->GetPort(), token, fingerprint));
    EXPECT_EQ(fingerprint, _panel->GetFingerprint());

    AdminClient const client("127.0.0.1", _panel->GetPort(), "", true);
    AdminClientResponse const served = client.Send({ "GET", "/api/health", "", "application/json", "" }, std::chrono::seconds(30));
    ASSERT_TRUE(served.Answered) << served.Error;
    EXPECT_EQ(served.PeerFingerprint, fingerprint) << "the pin is the certificate the listener actually serves";

    Browser paired;
    AdminClientResponse const traded = LinkIn(*_panel, paired, token);
    ASSERT_EQ(traded.Status, 200) << traded.Body;
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &paired).Status, 200);
    Browser again;
    EXPECT_EQ(LinkIn(*_panel, again, token).Status, 410);

    AdminClientResponse const bracketed = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "[::1]:8443" } });
    ASSERT_EQ(bracketed.Status, 200) << bracketed.Body;
    EXPECT_TRUE(Json(bracketed).value("line", std::string()).starts_with("https://[::1]:8443/#link?token="));
    EXPECT_EQ(Json(bracketed).value("host", std::string()), "::1");
}

TEST_F(PanelLinkTest, APairingNamesOnlyAnAddressThePanelAnswersFor)
{
    Start(Secure() + "Panel.AllowedHosts = panel.example\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    AdminClientResponse const stranger = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "other.example" } });
    EXPECT_EQ(stranger.Status, 422) << stranger.Body;
    nlohmann::json const fields = Json(stranger)["fields"];
    EXPECT_NE(fields.value("address", std::string()).find("Panel.AllowedHosts"), std::string::npos) << stranger.Body;
    for (char const* const address : { "not a host!", "127.0.0.1:0", "127.0.0.1:70000", "[::1", "0.0.0.0", "127.0.0.1:" })
    {
        AdminClientResponse const bad = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", address } });
        EXPECT_EQ(bad.Status, 422) << address << ": " << bad.Body;
        EXPECT_TRUE(Json(bad)["fields"].contains("address")) << address;
    }
    EXPECT_TRUE(Json(Mint({ { "kind", "pairing" }, { "address", "127.0.0.1" } }))["fields"].contains("username"));
    EXPECT_EQ(Mint({ { "kind", "pairing" }, { "username", "nobody" }, { "address", "127.0.0.1" } }).Status, 404);
    EXPECT_TRUE(Json(Mint({ { "kind", "remote" } }))["fields"].contains("kind"));
    EXPECT_TRUE(Json(Mint({ { "kind", "local" }, { "extra", true } }))["fields"].contains("extra"));

    AdminClientResponse const named = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "Panel.Example:9443" } });
    ASSERT_EQ(named.Status, 200) << named.Body;
    EXPECT_TRUE(Json(named).value("line", std::string()).starts_with("https://panel.example:9443/#link?token="));
}

TEST_F(PanelLinkTest, TwentyWrongOrSpentTokensFromOneAddressAreHeldBackAndADisabledOperatorsLinkIsRefused)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    std::string error;
    int64 helper = 0;
    ASSERT_EQ(_panel->Users().Create("helper", Password, false, false, &helper, error), PanelUserResult::Ok) << error;
    std::string const helpers = MintToken({ { "kind", "local" }, { "username", "helper" } });
    ASSERT_TRUE(_panel->Users().SetDisabled(helper, true, error)) << error;
    Browser disabled;
    AdminClientResponse const refused = LinkIn(*_panel, disabled, helpers);
    EXPECT_EQ(refused.Status, 403) << refused.Body;
    EXPECT_EQ(ErrorOf(refused), "link_refused");
    EXPECT_EQ(LinkIn(*_panel, disabled, helpers).Status, 410) << "a disabled operator's link was burned";
    EXPECT_NE(ColumnOfLast("reason", "panel:link.refused").find("already used"), std::string::npos);
    EXPECT_GE(PanelAudit::Count(_panel->Store(), "panel:link.refused"), 2);
    AdminClientResponse const notMinted = Mint({ { "kind", "local" }, { "username", "helper" } });
    EXPECT_EQ(notMinted.Status, 409) << notMinted.Body;
    EXPECT_EQ(ErrorOf(notMinted), "operator_disabled");

    _panel->LinkThrottle().Clear();
    AdminClientResponse const plain = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "127.0.0.1" } });
    ASSERT_EQ(plain.Status, 200) << plain.Body;
    EXPECT_TRUE(Json(plain)["fingerprint"].is_null()) << "a plain listener on loopback pins nothing";
    EXPECT_TRUE(Json(plain).value("line", std::string()).starts_with(fmt::format("http://127.0.0.1:{}/#link?token=", _panel->GetPort())));
    AdminClientResponse const remote = Mint({ { "kind", "pairing" }, { "username", "owner" }, { "address", "192.0.2.10" } });
    EXPECT_EQ(remote.Status, 409) << remote.Body;
    EXPECT_EQ(ErrorOf(remote), "plain_http_remote");

    std::string const spent = Json(plain).value("token", std::string());
    Browser first;
    ASSERT_EQ(LinkIn(*_panel, first, spent).Status, 200);
    for (int attempt = 0; attempt < 19; ++attempt)
    {
        Browser guesser;
        EXPECT_EQ(LinkIn(*_panel, guesser, PanelUsers::Unguessable()).Status, 403) << attempt;
    }
    Browser late;
    EXPECT_EQ(LinkIn(*_panel, late, spent).Status, 410);

    std::string const fresh = MintToken({ { "kind", "pairing" }, { "username", "owner" }, { "address", "127.0.0.1" } });
    Browser held;
    AdminClientResponse const throttled = LinkIn(*_panel, held, fresh);
    EXPECT_EQ(throttled.Status, 429) << throttled.Body;
    EXPECT_EQ(ErrorOf(throttled), "too_many_requests");
    EXPECT_NE(throttled.Head.find("Retry-After:"), std::string::npos) << throttled.Head;
    std::optional<PanelLink> const kept = LinkOf(fresh);
    ASSERT_TRUE(kept.has_value());
    EXPECT_FALSE(kept->UsedEpochMs.has_value()) << "a link held back is not spent";
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:link.throttled"), 1);

    Browser elsewhere;
    AdminClientResponse const other = LinkIn(*_panel, elsewhere, fresh, "203.0.113.9");
    EXPECT_EQ(other.Status, 200) << "another address is not held back by this one's count: " << other.Body;
}

TEST_F(PanelLinkTest, ALinkNeverSkipsTheSecondFactor)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::optional<Enrolled> const enrolled = Enroll(*_panel, owner, _seconds);
    ASSERT_TRUE(enrolled.has_value());
    _seconds += Totp::StepSeconds;

    std::string const token = MintToken({ { "kind", "local" } });
    Browser linked;
    AdminClientResponse const asked = LinkIn(*_panel, linked, token);
    ASSERT_EQ(asked.Status, 200) << asked.Body;
    EXPECT_TRUE(Json(asked).value("second_factor", false));
    EXPECT_FALSE(Json(asked).contains("csrf"));
    EXPECT_TRUE(SessionCookie(asked).empty()) << "a link alone opens no session for an operator with two-factor sign-in";
    ASSERT_FALSE(linked.Challenge.empty());
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &linked).Status, 401);
    EXPECT_EQ(PropertiesOfLast("panel:session.challenged").value("link_kind", std::string()), "local");

    AdminClientResponse const opened = SecondFactor(*_panel, linked, { { "code", CodeAt(enrolled->Secret, _seconds) } });
    ASSERT_EQ(opened.Status, 200) << opened.Body;
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/me", nullptr, &linked).Status, 200);
    nlohmann::json const properties = PropertiesOfLast("panel:session.opened");
    EXPECT_EQ(properties.value("link", std::string()), PanelLinks::HashOf(token).substr(0, PanelLinks::IdLength));
    EXPECT_EQ(properties.value("link_kind", std::string()), "local");
    EXPECT_EQ(properties.value("second_factor", std::string()), "totp");
    EXPECT_NE(ColumnOfLast("reason", "panel:session.opened").find("a local link, then a TOTP code"), std::string::npos);
}

TEST_F(PanelLinkTest, TheIssuedRowAndTheSessionRowNameTheLink)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    std::string const token = MintToken({ { "kind", "local" }, { "username", "owner" } });
    std::string const id = PanelLinks::HashOf(token).substr(0, PanelLinks::IdLength);
    nlohmann::json const issued = PropertiesOfLast("panel:link.issued");
    EXPECT_EQ(issued.value("kind", std::string()), "local");
    EXPECT_EQ(issued.value("user_id", int64{ 0 }), owner.UserId);
    EXPECT_EQ(issued.value("issuer", std::string()), "token");
    EXPECT_EQ(issued.value("expires_epoch_ms", int64{ 0 }), _now + 60000);
    EXPECT_EQ(issued.value("link", std::string()), id);
    EXPECT_EQ(ColumnOfLast("actor_type", "panel:link.issued"), "token");

    Browser opened;
    ASSERT_EQ(LinkIn(*_panel, opened, token).Status, 200);
    nlohmann::json const session = PropertiesOfLast("panel:session.opened");
    EXPECT_EQ(session.value("link", std::string()), id);
    EXPECT_EQ(session.value("link_kind", std::string()), "local");
    EXPECT_EQ(ColumnOfLast("reason", "panel:session.opened"), "a local link");

    std::string error;
    std::optional<PanelStore::Statement> subject = _panel->Store().Prepare(
        "SELECT COUNT(*) FROM audit_subject s JOIN audit_event e ON e.id = s.event WHERE e.name = 'panel:session.opened' AND s.kind = 'panel_link' AND s.subject_id = ?", error);
    ASSERT_TRUE(subject.has_value()) << error;
    subject->Bind(1, id);
    ASSERT_TRUE(subject->Step(error)) << error;
    EXPECT_EQ(subject->Int64(0), 1);
    subject.reset();

    std::optional<PanelLink> const link = LinkOf(token);
    ASSERT_TRUE(link.has_value());
    EXPECT_EQ(link->Kind, PanelLinkKind::Local);
    EXPECT_EQ(link->Issuer, "token");
    EXPECT_TRUE(link->UsedEpochMs.has_value());
    EXPECT_EQ(link->UsedAddress, "127.0.0.1");
}

TEST_F(PanelLinkTest, ATokenIsInNoLogFileAuditRowOrCommandHistoryAndTheStoreHoldsOnlyItsHash)
{
    ConsoleCommandTable commands;
    std::filesystem::path const record = _directory.Path() / "audit" / "supervisor-commands.jsonl";
    _prepare = [&commands, &record](Panel& panel)
    {
        PanelCommands::Register(commands, panel);
        AdminCommand::Register(panel.Routes(), commands, "supervisor", record);
    };
    Start(Secure());
    ASSERT_TRUE(_panel->IsSecure());
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));

    std::vector<std::string> tokens;
    tokens.push_back(MintToken({ { "kind", "local" } }));
    tokens.push_back(MintToken({ { "kind", "pairing" }, { "username", "owner" }, { "address", "127.0.0.1" } }));
    Transcript linked;
    ASSERT_EQ(commands.Execute("panel user link owner", linked.Reply()), ConsoleCommandTable::Result::Ran);
    ASSERT_EQ(linked.Lines.size(), 2u);
    tokens.push_back(TokenIn(linked.Lines[0]));
    Transcript paired;
    ASSERT_EQ(commands.Execute("panel user pair owner 127.0.0.1", paired.Reply()), ConsoleCommandTable::Result::Ran);
    ASSERT_EQ(paired.Lines.size(), 2u);
    tokens.push_back(TokenIn(paired.Lines[0]));
    EXPECT_NE(paired.Lines[1].find(_panel->GetFingerprint()), std::string::npos) << paired.Lines[1];
    for (std::string const& token : tokens)
    {
        ASSERT_EQ(token.size(), PanelLinks::TokenLength);
        Browser browser;
        EXPECT_EQ(LinkIn(*_panel, browser, token).Status, 200);
    }

    for (char const* const command : { "panel user link owner", "panel user pair owner 127.0.0.1" })
    {
        AdminClientResponse const relayed = Send(*_panel, "POST", "/api/command", { { "command", command } }, &owner);
        EXPECT_EQ(relayed.Status, 409) << relayed.Body;
        nlohmann::json const body = Json(relayed);
        EXPECT_TRUE(body.value("refused", false));
        EXPECT_EQ(body.value("reason", std::string()), AdminCommand::ConsoleOnlyReason);
        EXPECT_TRUE(body["lines"].empty()) << relayed.Body;
    }

    std::vector<std::string> const found = ScanFor(*_panel, _harness, tokens);
    EXPECT_TRUE(found.empty()) << found.front();
    std::string const history = ReadAll(record);
    ASSERT_NE(history.find("panel user link owner"), std::string::npos) << "the command record keeps the line";
    std::string const store = ReadAll(_panel->Store().GetFile());
    std::string const journal = ReadAll(std::filesystem::path(_panel->Store().GetFile()).concat("-wal"));
    std::string const keyring = ReadAll(_panel->Keyring().GetFile());
    ASSERT_FALSE(store.empty());
    for (std::string const& token : tokens)
    {
        EXPECT_EQ(history.find(token), std::string::npos) << "the command record holds a token";
        EXPECT_EQ(store.find(token), std::string::npos) << "the store holds a token";
        EXPECT_EQ(journal.find(token), std::string::npos) << "the store's write-ahead log holds a token";
        EXPECT_EQ(keyring.find(token), std::string::npos);
    }

    std::string error;
    std::optional<PanelStore::Statement> rows = _panel->Store().Prepare(
        "SELECT id, kind, token_hash, user_id, issuer, COALESCE(address, ''), created_epoch_ms, expires_epoch_ms, used_epoch_ms, COALESCE(used_address, '') FROM panel_link", error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::set<std::string> hashes;
    while (rows->Step(error))
    {
        hashes.insert(rows->Text(2));
        EXPECT_EQ(rows->Text(0), rows->Text(2).substr(0, PanelLinks::IdLength));
        for (int column = 0; column < 10; ++column)
            for (std::string const& token : tokens)
                EXPECT_EQ(rows->Text(column).find(token), std::string::npos) << "panel_link column " << column << " holds a token";
    }
    rows.reset();
    for (std::string const& token : tokens)
        EXPECT_TRUE(hashes.contains(PanelLinks::HashOf(token))) << "the store keeps each link's SHA-256";
}

TEST_F(PanelLinkTest, TheCommandLineReadsTheTokenFileAndPrintsOnlyTheLink)
{
    Start();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::filesystem::path const tokenFile = _directory.Write("admin.token", Bearer);
    std::filesystem::path const conf = _directory.Write("cli.conf", fmt::format("Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {}\nAdmin.TokenFile = \"{}\"\n",
        _admin->GetPort(), tokenFile.generic_string()));
    std::string const config = conf.generic_string();
    auto const run = [this](std::vector<std::string> const& arguments, std::string& out, std::string& err)
    {
        ConfigMgr loaded(NoEnvironment());
        std::ostringstream printed;
        std::ostringstream said;
        int const code = PanelLinkClient::Run(arguments, loaded, _directory.Path() / "data", 12020, printed, said);
        out = printed.str();
        err = said.str();
        return code;
    };

    std::string out;
    std::string err;
    ASSERT_EQ(run({ "supervisor", "-c", config, "--panel-link" }, out, err), 0) << err;
    ASSERT_TRUE(out.ends_with('\n'));
    EXPECT_EQ(std::count(out.begin(), out.end(), '\n'), 1) << out;
    std::string const link = out.substr(0, out.size() - 1);
    EXPECT_TRUE(link.starts_with(fmt::format("http://127.0.0.1:{}/#link?token=", _panel->GetPort()))) << link;
    EXPECT_NE(err.find("owner"), std::string::npos) << err;
    EXPECT_EQ(err.find(TokenIn(link)), std::string::npos) << "the token goes to standard output alone";
    Browser opened;
    AdminClientResponse const traded = LinkIn(*_panel, opened, TokenIn(link));
    ASSERT_EQ(traded.Status, 200) << traded.Body;
    EXPECT_EQ(Json(traded)["user"].value("username", std::string()), "owner");

    ASSERT_EQ(run({ "supervisor", "--panel-pair", "owner", "--address", "127.0.0.1", "--config", config }, out, err), 0) << err;
    EXPECT_TRUE(out.starts_with(fmt::format("http://127.0.0.1:{}/#link?token=", _panel->GetPort()))) << out;
    EXPECT_EQ(std::count(out.begin(), out.end(), '\n'), 1) << out;

    EXPECT_EQ(run({ "supervisor", "-c", config, "--panel-pair", "owner" }, out, err), 2);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(run({ "supervisor", "-c", config, "--panel-link", "--address", "127.0.0.1" }, out, err), 2);
    EXPECT_EQ(run({ "supervisor", "-c", config, "--panel-link", "--nonsense" }, out, err), 2);

    std::filesystem::path const missing = _directory.Path() / "nowhere" / "admin.token";
    std::string const elsewhere = _directory.Write("missing.conf", fmt::format("Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {}\nAdmin.TokenFile = \"{}\"\n",
        _admin->GetPort(), missing.generic_string())).generic_string();
    EXPECT_EQ(run({ "supervisor", "-c", elsewhere, "--panel-link" }, out, err), 1);
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(std::filesystem::exists(missing)) << "asking for a link never writes a token";

    _directory.Write("admin.token", "fedcba9876543210fedcba9876543210");
    EXPECT_EQ(run({ "supervisor", "-c", config, "--panel-link" }, out, err), 1);
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(err.empty());
}

TEST_F(PanelLinkTest, APairingTokenSignsInOnceOverTlsFromBeyondLoopback)
{
    std::optional<std::string> const named = Ambrose::GetEnv("AMBROSE_TEST_ADMIN_REMOTE_BIND");
    if (!named || named->empty())
        GTEST_SKIP() << "AMBROSE_TEST_ADMIN_REMOTE_BIND names no address to bind";
    std::optional<asio::ip::address> const bound = Ambrose::Asio::MakeAddress(*named);
    ASSERT_TRUE(bound.has_value()) << *named << " is not an address";
    bool const everywhere = Ambrose::Asio::IsUnspecified(*bound);
    std::optional<std::string> const target = everywhere ? SourceTowards(*bound) : std::optional<std::string>(bound->to_string());
    if (!target || Ambrose::Asio::IsLoopback(*Ambrose::Asio::MakeAddress(*target)))
        GTEST_SKIP() << "this machine has no address beyond loopback to reach the panel from";

    Start(fmt::format("Panel.BindIP = {}\n", *named) + Secure());
    AdminClientResponse const minted = Mint({ { "kind", "pairing" }, { "username", "far_owner" }, { "address", *target } });
    ASSERT_EQ(minted.Status, 200) << minted.Body;
    nlohmann::json const answer = Json(minted);
    EXPECT_TRUE(answer.value("created_owner", false));
    std::string const token = answer.value("token", std::string());
    EXPECT_EQ(answer.value("fingerprint", std::string()), _panel->GetFingerprint());

    AdminClient const remoteClient(*target, _panel->GetPort(), "", true);
    AdminClientRequest const trade{ "POST", std::string(Panel::LinkPath), nlohmann::json{ { "token", token } }.dump(), "application/json", "" };
    AdminClientResponse const traded = remoteClient.Send(trade, std::chrono::seconds(30));
    ASSERT_TRUE(traded.Answered) << traded.Error;
    ASSERT_EQ(traded.Status, 200) << traded.Body;
    EXPECT_EQ(traded.PeerFingerprint, answer.value("fingerprint", std::string()));
    EXPECT_FALSE(SessionCookie(traded).empty());
    std::optional<asio::ip::address> const from = Ambrose::Asio::MakeAddress(ColumnOfLast("address", "panel:session.opened"));
    ASSERT_TRUE(from.has_value());
    EXPECT_FALSE(Ambrose::Asio::IsLoopback(*from)) << from->to_string();
    EXPECT_EQ(remoteClient.Send(trade, std::chrono::seconds(30)).Status, 410);

    AdminClientResponse const local = Mint({ { "kind", "local" } });
    if (everywhere)
    {
        ASSERT_EQ(local.Status, 200) << local.Body;
        AdminClientRequest const tryLocal{ "POST", std::string(Panel::LinkPath), nlohmann::json{ { "token", Json(local).value("token", std::string()) } }.dump(), "application/json", "" };
        AdminClientResponse const refused = remoteClient.Send(tryLocal, std::chrono::seconds(30));
        EXPECT_EQ(refused.Status, 403) << refused.Body;
        EXPECT_EQ(ErrorOf(refused), "not_this_machine");
    }
    else
    {
        EXPECT_EQ(local.Status, 409) << local.Body;
        EXPECT_EQ(ErrorOf(local), "not_on_loopback");
    }
}

TEST_F(PanelLinkTest, APairingOfAPlainListenerBeyondLoopbackIsRefusedWithTheReason)
{
    std::optional<std::string> const named = Ambrose::GetEnv("AMBROSE_TEST_ADMIN_REMOTE_BIND");
    if (!named || named->empty())
        GTEST_SKIP() << "AMBROSE_TEST_ADMIN_REMOTE_BIND names no address to bind";

    Start(fmt::format("Panel.BindIP = {}\nPanel.AllowPlainHttpRemote = 1\n", *named));
    AdminClientResponse const refused = Mint({ { "kind", "pairing" } });
    EXPECT_EQ(refused.Status, 409) << refused.Body;
    EXPECT_EQ(ErrorOf(refused), "plain_http_remote");
    std::string const message = Json(refused).value("message", std::string());
    EXPECT_NE(message.find("Panel.CertificateFile"), std::string::npos) << message;
    EXPECT_NE(message.find("--panel-self-signed"), std::string::npos) << message;
}
