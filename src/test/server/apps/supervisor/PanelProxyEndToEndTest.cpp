/*
 * Project Ambrose by Imjustchico
 * Proves the forwarded-header check end to end: behind a test proxy missing from the trusted list the check says the peer is untrusted and a problem record is written, the audit row of a settings change carries the proxy's address, and once the page has set Panel.TrustedProxies the check and the next audit row show the real client address.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelStore.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class ForwardingProxy
    {
    public:
        ForwardingProxy(uint16 backendPort, std::string forwardedFor)
            : _acceptor(_io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)),
              _backendPort(backendPort), _forwardedFor(std::move(forwardedFor))
        {
            _thread = std::thread([this] { AcceptLoop(); });
        }

        ~ForwardingProxy() { Stop(); }

        ForwardingProxy(ForwardingProxy const&) = delete;
        ForwardingProxy& operator=(ForwardingProxy const&) = delete;

        uint16 Port() const { return _acceptor.local_endpoint().port(); }

        void Stop()
        {
            std::error_code ec;
            _acceptor.close(ec);
            if (_thread.joinable())
                _thread.join();
        }

    private:
        void AcceptLoop()
        {
            for (;;)
            {
                asio::ip::tcp::socket client(_io);
                std::error_code ec;
                _acceptor.accept(client, ec);
                if (ec)
                    return;
                std::thread([this, socket = std::move(client)]() mutable { Handle(std::move(socket)); }).detach();
            }
        }

        void Handle(asio::ip::tcp::socket client)
        {
            try
            {
                asio::ip::tcp::socket backend(_io);
                backend.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), _backendPort));
                asio::streambuf head;
                asio::read_until(client, head, "\r\n\r\n");
                std::string all(asio::buffers_begin(head.data()), asio::buffers_end(head.data()));
                std::size_t const end = all.find("\r\n\r\n") + 4;
                std::string headerPart = all.substr(0, end);
                std::string const bodyPart = all.substr(end);
                headerPart.insert(headerPart.size() - 2, "\r\nX-Forwarded-For: " + _forwardedFor);
                asio::write(backend, asio::buffer(headerPart + bodyPart));
                std::thread relayOut([&client, &backend] { Relay(client, backend); });
                Relay(backend, client);
                relayOut.join();
            }
            catch (...)
            {
            }
        }

        static void Relay(asio::ip::tcp::socket& from, asio::ip::tcp::socket& to)
        {
            char buffer[8192];
            std::error_code ec;
            for (;;)
            {
                std::size_t const n = from.read_some(asio::buffer(buffer), ec);
                if (ec)
                    break;
                asio::write(to, asio::buffer(buffer, n), ec);
                if (ec)
                    break;
            }
            std::error_code ignored;
            to.shutdown(asio::ip::tcp::socket::shutdown_send, ignored);
        }

        asio::io_context _io;
        asio::ip::tcp::acceptor _acceptor;
        uint16 _backendPort;
        std::string _forwardedFor;
        std::thread _thread;
    };

    class PanelProxyEndToEndTest : public testing::Test
    {
    protected:
        ConfigMgr& Configured(std::string const& text)
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            EXPECT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", text)).Succeeded());
            return *_config;
        }

        Panel Make()
        {
            return Panel(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
        }

        std::string AuditAddress(Panel& panel, std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows =
                panel.Store().Prepare("SELECT address FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            EXPECT_TRUE(rows.has_value()) << error;
            rows->Bind(1, name);
            EXPECT_TRUE(rows->Step(error)) << error;
            return rows->Text(0);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST_F(PanelProxyEndToEndTest, UntrustedProxyIsFlaggedThenTrustedProxyRevealsTheClient)
{    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\n"), error)) << error;

    int64 ownerId = 0;
    ASSERT_EQ(panel.Users().Create("proxy-owner", "a good long password", true, false, &ownerId, error), PanelUserResult::Ok) << error;
    std::optional<PanelSessionOpened> const ownerSession = panel.Sessions().Open(ownerId, 1, "127.0.0.1", "test", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(ownerSession.has_value()) << error;
    std::string const cookie = panel.Routes().GetBrowserAccess().CookieName + "=" + ownerSession->Secret;
    std::string const csrf = ownerSession->Csrf;

    ForwardingProxy proxy(panel.GetPort(), "203.0.113.7");
    AdminClient const client("127.0.0.1", proxy.Port(), "");
    auto const sendAsOwner = [&](std::string method, std::string path, std::string body)
    {
        AdminClientRequest request{ std::move(method), std::move(path), std::move(body), "application/json", "" };
        request.Headers.emplace_back("Cookie", cookie);
        request.Headers.emplace_back("Origin", "http://127.0.0.1:" + std::to_string(proxy.Port()));
        request.Headers.emplace_back("X-CSRF-Token", csrf);
        return client.Send(request, std::chrono::seconds(10));
    };

    AdminClientResponse const before = sendAsOwner("GET", "/api/panel/proxy/check", "");
    ASSERT_TRUE(before.Answered) << before.Error;
    ASSERT_EQ(before.Status, 200) << before.Body;
    nlohmann::json const beforeCheck = nlohmann::json::parse(before.Body);
    EXPECT_EQ(beforeCheck["peer"], "127.0.0.1");
    EXPECT_FALSE(beforeCheck["peer_trusted"].get<bool>());
    EXPECT_EQ(beforeCheck["client_address"], "127.0.0.1");
    bool sawUntrusted = false;
    for (auto const& problem : beforeCheck["problems"])
        if (problem["code"] == "untrusted_proxy")
            sawUntrusted = true;
    EXPECT_TRUE(sawUntrusted) << before.Body;

    EXPECT_EQ(AuditAddress(panel, "panel:proxy.untrusted_forwarded"), "127.0.0.1");

    AdminClientResponse const trusted = sendAsOwner("PATCH", "/api/panel/settings",
        nlohmann::json({ { "values", { { "Panel.TrustedProxies", "127.0.0.1" } } } }).dump());
    ASSERT_TRUE(trusted.Answered) << trusted.Error;
    ASSERT_EQ(trusted.Status, 200) << trusted.Body;
    EXPECT_EQ(AuditAddress(panel, "panel:settings.changed"), "127.0.0.1");

    AdminClientResponse const after = sendAsOwner("GET", "/api/panel/proxy/check", "");
    ASSERT_TRUE(after.Answered) << after.Error;
    ASSERT_EQ(after.Status, 200) << after.Body;
    nlohmann::json const afterCheck = nlohmann::json::parse(after.Body);
    EXPECT_TRUE(afterCheck["peer_trusted"].get<bool>());
    EXPECT_EQ(afterCheck["client_address"], "203.0.113.7");
    EXPECT_TRUE(afterCheck["problems"].empty()) << after.Body;

    AdminClientResponse const renamed = sendAsOwner("PATCH", "/api/panel/settings",
        nlohmann::json({ { "values", { { "Panel.PublicUrl", "https://panel.example.com" } } } }).dump());
    ASSERT_TRUE(renamed.Answered) << renamed.Error;
    ASSERT_EQ(renamed.Status, 200) << renamed.Body;
    EXPECT_EQ(AuditAddress(panel, "panel:settings.changed"), "203.0.113.7");

    panel.Stop();
}

TEST_F(PanelProxyEndToEndTest, TrustedProxyChangeIsAuditedWithOldAndNewAndRefusedWithoutPermission)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\n"), error)) << error;

    int64 ownerId = 0;
    ASSERT_EQ(panel.Users().Create("proxy-owner", "a good long password", true, false, &ownerId, error), PanelUserResult::Ok) << error;
    std::optional<PanelSessionOpened> const ownerSession = panel.Sessions().Open(ownerId, 1, "127.0.0.1", "test", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(ownerSession.has_value()) << error;
    std::string const ownerCookie = panel.Routes().GetBrowserAccess().CookieName + "=" + ownerSession->Secret;

    int64 operatorId = 0;
    ASSERT_EQ(panel.Users().Create("proxy-operator", "another good password", false, false, &operatorId, error), PanelUserResult::Ok) << error;
    std::optional<PanelSessionOpened> const operatorSession = panel.Sessions().Open(operatorId, 1, "127.0.0.1", "test", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(operatorSession.has_value()) << error;
    std::string const operatorCookie = panel.Routes().GetBrowserAccess().CookieName + "=" + operatorSession->Secret;

    AdminClient const client("127.0.0.1", panel.GetPort(), "");
    auto const send = [&](std::string const& cookie, std::string const& csrf, std::string method, std::string path, std::string body)
    {
        AdminClientRequest request{ std::move(method), std::move(path), std::move(body), "application/json", "" };
        request.Headers.emplace_back("Cookie", cookie);
        request.Headers.emplace_back("Origin", "http://127.0.0.1:" + std::to_string(panel.GetPort()));
        request.Headers.emplace_back("X-CSRF-Token", csrf);
        return client.Send(request, std::chrono::seconds(10));
    };

    AdminClientResponse const refused = send(operatorCookie, operatorSession->Csrf, "PATCH", "/api/panel/settings",
        nlohmann::json({ { "values", { { "Panel.TrustedProxies", "127.0.0.1" } } } }).dump());
    ASSERT_TRUE(refused.Answered) << refused.Error;
    EXPECT_EQ(refused.Status, 403) << refused.Body;

    AdminClientResponse const changed = send(ownerCookie, ownerSession->Csrf, "PATCH", "/api/panel/settings",
        nlohmann::json({ { "values", { { "Panel.TrustedProxies", "127.0.0.1" } } } }).dump());
    ASSERT_TRUE(changed.Answered) << changed.Error;
    ASSERT_EQ(changed.Status, 200) << changed.Body;

    std::optional<PanelStore::Statement> rows =
        panel.Store().Prepare("SELECT properties FROM audit_event WHERE name = 'panel:settings.changed' ORDER BY id DESC LIMIT 1", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    nlohmann::json const properties = nlohmann::json::parse(rows->Text(0));
    ASSERT_TRUE(properties.contains("changes")) << properties.dump();
    ASSERT_TRUE(properties["changes"].contains("Panel.TrustedProxies")) << properties.dump();
    EXPECT_EQ(properties["changes"]["Panel.TrustedProxies"]["old"], "");
    EXPECT_EQ(properties["changes"]["Panel.TrustedProxies"]["new"], "127.0.0.1");

    panel.Stop();
}
