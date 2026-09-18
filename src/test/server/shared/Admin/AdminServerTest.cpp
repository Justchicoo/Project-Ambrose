/*
 * Project Ambrose by Imjustchico
 * Tests the admin API listener on a loopback port the operating system picks: health needs the token, wrong tokens are rate limited, every path answers the same way without one, a reload rotates the token without a restart and keeps the old listener when the new port is taken, an unsafe remote bind is refused, WebSocket routes registered before or after the listener opens take the same token and carry frames both ways, and an app whose admin binding is unsafe exits with a failure.
 */

#include "AdminServer.h"
#include "AdminSettings.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "GitRevision.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ServerApp.h"

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";
    constexpr char const* OtherToken = "fedcba9876543210fedcba9876543210";

    struct HttpReply
    {
        int Status = 0;
        std::string Head;
        std::string Body;
    };

    std::optional<std::size_t> ContentLength(std::string const& head)
    {
        std::string lowered;
        lowered.reserve(head.size());
        for (char const character : head)
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        std::size_t const position = lowered.find("content-length:");
        if (position == std::string::npos)
            return std::nullopt;
        std::size_t const end = lowered.find("\r\n", position);
        std::string const value = head.substr(position + 15, end - position - 15);
        try
        {
            return static_cast<std::size_t>(std::stoul(value));
        }
        catch (std::exception const&)
        {
            return std::nullopt;
        }
    }

    HttpReply Send(uint16 port, std::string const& request, std::string const& address = "127.0.0.1")
    {
        HttpReply reply;
        asio::io_context context;
        asio::ip::tcp::socket socket(context);
        std::error_code code;
        socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address(address), port), code);
        if (code)
            return reply;
        asio::write(socket, asio::buffer(request), code);
        if (code)
            return reply;

        std::string data;
        std::array<char, 2048> chunk{};
        std::size_t headEnd = std::string::npos;
        std::size_t expected = std::string::npos;
        for (;;)
        {
            if (headEnd == std::string::npos)
                headEnd = data.find("\r\n\r\n");
            if (headEnd != std::string::npos)
            {
                if (expected == std::string::npos)
                {
                    std::optional<std::size_t> const length = ContentLength(data.substr(0, headEnd + 4));
                    expected = length.value_or(0);
                }
                if (data.size() >= headEnd + 4 + expected)
                    break;
            }
            std::size_t const read = socket.read_some(asio::buffer(chunk), code);
            data.append(chunk.data(), read);
            if (code || read == 0)
                break;
        }
        socket.close(code);

        if (headEnd == std::string::npos)
            headEnd = data.find("\r\n\r\n");
        reply.Head = headEnd == std::string::npos ? data : data.substr(0, headEnd);
        reply.Body = headEnd == std::string::npos ? std::string() : data.substr(headEnd + 4);
        std::size_t const space = reply.Head.find(' ');
        if (space != std::string::npos)
            reply.Status = std::atoi(reply.Head.c_str() + space + 1);
        return reply;
    }

    HttpReply Ask(uint16 port, std::string const& method, std::string const& path, std::string const& token, std::string const& body = std::string(), std::string const& address = "127.0.0.1")
    {
        std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
        if (!token.empty())
            request += "Authorization: Bearer " + token + "\r\n";
        if (!body.empty())
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        request += "\r\n" + body;
        return Send(port, request, address);
    }

    HttpReply Get(uint16 port, std::string const& path, std::string const& token)
    {
        return Ask(port, "GET", path, token);
    }

    std::string UpgradeRequest(std::string const& path, std::string const& token)
    {
        std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n";
        if (!token.empty())
            request += "Authorization: Bearer " + token + "\r\n";
        request += "\r\n";
        return request;
    }

    HttpReply Upgrade(uint16 port, std::string const& path, std::string const& token)
    {
        return Send(port, UpgradeRequest(path, token));
    }

    class SocketClient
    {
    public:
        ~SocketClient()
        {
            std::error_code code;
            _socket.close(code);
        }

        bool Open(uint16 port, std::string const& path, std::string const& token)
        {
            std::error_code code;
            _socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), code);
            if (code)
                return false;
            asio::write(_socket, asio::buffer(UpgradeRequest(path, token)), code);
            if (code)
                return false;
            std::size_t head = _buffer.find("\r\n\r\n");
            while (head == std::string::npos)
            {
                if (!Fill())
                    return false;
                head = _buffer.find("\r\n\r\n");
            }
            std::string const reply = _buffer.substr(0, head);
            _buffer.erase(0, head + 4);
            return reply.find(" 101 ") != std::string::npos;
        }

        bool SendText(std::string const& text) { return SendFrame(0x81, text); }

        bool SendClose(uint16 status)
        {
            std::string payload;
            payload.push_back(static_cast<char>(status >> 8));
            payload.push_back(static_cast<char>(status & 0xFF));
            return SendFrame(0x88, payload);
        }

        std::optional<std::string> ReadText()
        {
            for (;;)
            {
                std::optional<std::pair<uint8, std::string>> const frame = ReadFrame();
                if (!frame)
                    return std::nullopt;
                if (frame->first == 0x1)
                    return frame->second;
                if (frame->first == 0x8)
                    return std::nullopt;
            }
        }

        std::optional<std::pair<uint8, std::string>> ReadFrame()
        {
            while (_buffer.size() < 2)
                if (!Fill())
                    return std::nullopt;
            uint8 const opcode = static_cast<uint8>(_buffer[0]) & 0x0F;
            std::size_t length = static_cast<uint8>(_buffer[1]) & 0x7F;
            std::size_t header = 2;
            if (length == 126)
            {
                while (_buffer.size() < 4)
                    if (!Fill())
                        return std::nullopt;
                length = (static_cast<std::size_t>(static_cast<uint8>(_buffer[2])) << 8) | static_cast<uint8>(_buffer[3]);
                header = 4;
            }
            while (_buffer.size() < header + length)
                if (!Fill())
                    return std::nullopt;
            std::string const payload = _buffer.substr(header, length);
            _buffer.erase(0, header + length);
            return std::make_pair(opcode, payload);
        }

    private:
        bool SendFrame(uint8 header, std::string const& payload)
        {
            std::array<uint8, 4> const mask{ 0x12, 0x34, 0x56, 0x78 };
            std::string frame;
            frame.push_back(static_cast<char>(header));
            frame.push_back(static_cast<char>(0x80 | static_cast<uint8>(payload.size())));
            for (uint8 const byte : mask)
                frame.push_back(static_cast<char>(byte));
            for (std::size_t index = 0; index < payload.size(); ++index)
                frame.push_back(static_cast<char>(static_cast<uint8>(payload[index]) ^ mask[index % mask.size()]));
            std::error_code code;
            asio::write(_socket, asio::buffer(frame), code);
            return !code;
        }

        bool Fill()
        {
            std::array<char, 2048> chunk{};
            std::optional<std::error_code> result;
            std::size_t read = 0;
            asio::steady_timer timer(_context);
            _socket.async_read_some(asio::buffer(chunk), [&](std::error_code code, std::size_t size)
            {
                result = code;
                read = size;
                timer.cancel();
            });
            timer.expires_after(std::chrono::seconds(10));
            timer.async_wait([&](std::error_code code)
            {
                if (code == asio::error::operation_aborted)
                    return;
                std::error_code ignored;
                _socket.cancel(ignored);
            });
            _context.restart();
            _context.run();
            if (!result || *result)
                return false;
            _buffer.append(chunk.data(), read);
            return read != 0;
        }

        asio::io_context _context;
        asio::ip::tcp::socket _socket{ _context };
        std::string _buffer;
    };

    class AdminServerTest : public testing::Test
    {
    protected:
        AdminSettings Loopback() const
        {
            AdminSettings settings;
            settings.Enable = true;
            settings.BindIp = "127.0.0.1";
            settings.Port = 0;
            settings.Token = Token;
            settings.AuthFailureBurst = 10;
            settings.AuthFailuresPerSecond = 0.0;
            return settings;
        }

        AdminServer Make()
        {
            return AdminServer(_harness.GetLog(), "testserver", _directory.Path());
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
    };
}

TEST_F(AdminServerTest, ServesHealthOnLoopbackOnlyWithTheToken)
{
    AdminServer server = Make();
    server.SetHealthSource([]
    {
        AdminHealth health;
        health.App = "testserver";
        health.Realm = "Ambrose";
        health.Revision = GitRevision::GetHash();
        health.UptimeSeconds = 7;
        health.State = "running";
        return health;
    });

    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    ASSERT_TRUE(server.IsRunning());
    ASSERT_NE(server.GetPort(), 0);
    EXPECT_EQ(server.GetBindIp(), "127.0.0.1");

    HttpReply const missing = Get(server.GetPort(), "/api/health", "");
    EXPECT_EQ(missing.Status, 401);
    EXPECT_NE(missing.Head.find("WWW-Authenticate: Bearer"), std::string::npos);

    HttpReply const allowed = Get(server.GetPort(), "/api/health", Token);
    ASSERT_EQ(allowed.Status, 200) << allowed.Head;
    nlohmann::json const body = nlohmann::json::parse(allowed.Body);
    EXPECT_EQ(body["app"], "testserver");
    EXPECT_EQ(body["realm"], "Ambrose");
    EXPECT_EQ(body["revision"], GitRevision::GetHash());
    EXPECT_EQ(body["uptime"], 7);
    EXPECT_EQ(body["state"], "running");

    EXPECT_EQ(Get(server.GetPort(), "/api/nothing", Token).Status, 404);

    server.Stop();
    EXPECT_FALSE(server.IsRunning());
    EXPECT_EQ(server.GetPort(), 0);
}

TEST_F(AdminServerTest, RateLimitsWrongTokens)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;

    int limited = 0;
    for (int attempt = 0; attempt < 20; ++attempt)
        if (Get(server.GetPort(), "/api/health", "wrongwrongwrongwrong").Status == 429)
            ++limited;
    EXPECT_EQ(limited, 10);
    EXPECT_EQ(Get(server.GetPort(), "/api/health", Token).Status, 429);
}

TEST_F(AdminServerTest, RotatesTheTokenOnReloadWithoutRestarting)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();
    ASSERT_EQ(Get(port, "/api/health", Token).Status, 200);

    AdminSettings rotated = Loopback();
    rotated.Port = port;
    rotated.Token = OtherToken;
    ASSERT_TRUE(server.Reload(rotated));
    EXPECT_EQ(server.GetPort(), port);
    EXPECT_EQ(server.GetToken(), OtherToken);
    EXPECT_EQ(Get(port, "/api/health", Token).Status, 401);
    EXPECT_EQ(Get(port, "/api/health", OtherToken).Status, 200);
}

TEST_F(AdminServerTest, ChangingTheThreadCountRebindsTheSamePort)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();
    ASSERT_EQ(Get(port, "/api/health", Token).Status, 200);

    AdminSettings threads = Loopback();
    threads.Threads = 4;
    ASSERT_TRUE(server.Reload(threads));
    EXPECT_EQ(server.GetPort(), port);
    EXPECT_EQ(Get(port, "/api/health", Token).Status, 200);
}

TEST_F(AdminServerTest, ReloadKeepsTheOldListenerWhenTheNewBindIsUnsafe)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();

    AdminSettings remote = Loopback();
    remote.Port = port;
    remote.BindIp = "0.0.0.0";
    EXPECT_FALSE(server.Reload(remote));
    EXPECT_TRUE(server.IsRunning());
    EXPECT_EQ(server.GetPort(), port);
    EXPECT_EQ(Get(port, "/api/health", Token).Status, 200);

    AdminSettings off = Loopback();
    off.Enable = false;
    EXPECT_TRUE(server.Reload(off));
    EXPECT_FALSE(server.IsRunning());
}

TEST_F(AdminServerTest, RefusesAnUnsafeRemoteBindAtStart)
{
    AdminServer server = Make();
    AdminSettings settings = Loopback();
    settings.BindIp = "0.0.0.0";
    std::string error;
    EXPECT_FALSE(server.Start(settings, error));
    EXPECT_NE(error.find("Admin.AllowPlainHttpRemote"), std::string::npos);
    EXPECT_FALSE(server.IsRunning());
}

TEST_F(AdminServerTest, StaysOffWhenDisabled)
{
    AdminServer server = Make();
    AdminSettings settings = Loopback();
    settings.Enable = false;
    std::string error;
    EXPECT_TRUE(server.Start(settings, error)) << error;
    EXPECT_FALSE(server.IsRunning());
    EXPECT_EQ(server.GetPort(), 0);
}

TEST_F(AdminServerTest, AuthenticatesWebSocketUpgrades)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    AdminSocketRoute route;
    route.Path = "/api/socket";
    server.AddSocket(route);

    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    EXPECT_EQ(Upgrade(server.GetPort(), "/api/socket", "").Status, 401);
    EXPECT_EQ(Upgrade(server.GetPort(), "/api/socket", Token).Status, 101);
    EXPECT_NE(Upgrade(server.GetPort(), "/api/nowhere", Token).Status, 101);
}

TEST_F(AdminServerTest, GeneratesATokenWhenConfigHasNone)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    AdminSettings settings = Loopback();
    settings.Token.clear();

    std::string error;
    ASSERT_TRUE(server.Start(settings, error)) << error;
    EXPECT_EQ(server.GetToken().size(), 64u);
    EXPECT_TRUE(std::filesystem::is_regular_file(_directory.Path() / "admin" / "testserver.token"));
    EXPECT_EQ(Get(server.GetPort(), "/api/health", server.GetToken()).Status, 200);
}

TEST_F(AdminServerTest, AnAppRefusesToStartWithAnUnsafeAdminBind)
{
    std::filesystem::path const file = _directory.Write("testserver.conf",
        "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\nConsole.Enable = 0\n"
        "Admin.Enable = 1\nAdmin.BindIP = 0.0.0.0\nAdmin.Port = 0\nAdmin.Token = 0123456789abcdef0123456789abcdef\n");
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    std::ostringstream out;
    std::ostringstream err;
    ServerApp app({ "testserver", "testserver.conf", 0 }, config, _harness.GetLog(), out, err);

    EXPECT_EQ(app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file), "--check" }), EXIT_FAILURE);
    EXPECT_NE(err.str().find("Admin.AllowPlainHttpRemote"), std::string::npos) << err.str();
    EXPECT_EQ(app.GetLifecycleState(), AppLifecycle::Stopped);
}

TEST_F(AdminServerTest, AnAppServesItsOwnHealthWhileRunning)
{
    std::filesystem::path const file = _directory.Write("testserver.conf",
        "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\nConsole.Enable = 0\n"
        "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 0\nAdmin.Token = 0123456789abcdef0123456789abcdef\n");
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    std::ostringstream out;
    std::ostringstream err;
    ServerApp app({ "testserver", "testserver.conf", 0 }, config, _harness.GetLog(), out, err);

    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file) }); });

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!app.IsReady() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(app.IsReady());
    ASSERT_NE(app.GetAdminApi(), nullptr);
    ASSERT_NE(app.GetAdminApi()->GetPort(), 0);

    HttpReply const reply = Get(app.GetAdminApi()->GetPort(), "/api/health", Token);
    ASSERT_EQ(reply.Status, 200) << reply.Head;
    nlohmann::json const body = nlohmann::json::parse(reply.Body);
    EXPECT_EQ(body["app"], "testserver");
    EXPECT_EQ(body["realm"], "");
    EXPECT_EQ(body["revision"], GitRevision::GetHash());
    EXPECT_EQ(body["state"], "running");

    app.RequestStop();
    runner.join();
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
    EXPECT_EQ(app.GetAdminApi(), nullptr);
}

TEST_F(AdminServerTest, AnswersEveryPathTheSameWayWithoutTheToken)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    AdminSocketRoute route;
    route.Path = "/api/socket";
    server.AddSocket(route);

    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();

    std::vector<std::string> const paths{ "/api/health", "/api/socket", "/api/nothing", "/static/notes.txt", "/" };
    for (std::string const& path : paths)
    {
        HttpReply const refused = Ask(port, "GET", path, "");
        EXPECT_EQ(refused.Status, 401) << path << " " << refused.Head;
        EXPECT_NE(refused.Head.find("WWW-Authenticate: Bearer"), std::string::npos) << path;
        EXPECT_EQ(Ask(port, "OPTIONS", path, "").Status, 401) << path;
        EXPECT_EQ(Ask(port, "POST", path, "").Status, 401) << path;
    }
}

TEST_F(AdminServerTest, ServesNoStaticFilesEvenWithTheToken)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;

    HttpReply const refused = Get(server.GetPort(), "/static/notes.txt", Token);
    EXPECT_EQ(refused.Status, 404) << refused.Head;
    EXPECT_NE(refused.Body.find("not_found"), std::string::npos) << refused.Body;
}

TEST_F(AdminServerTest, RefusesARequestBodyOverTheLimit)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    AdminSettings settings = Loopback();
    settings.MaxRequestBytes = AdminSettings::MinRequestBytes;
    std::string error;
    ASSERT_TRUE(server.Start(settings, error)) << error;

    server.Routes().Add("POST", "/api/echo", [](AdminRequest const& request) { return AdminResponse::Json(200, request.Body); });
    EXPECT_EQ(Ask(server.GetPort(), "POST", "/api/echo", Token, std::string(16, 'a')).Status, 200);
    EXPECT_EQ(Ask(server.GetPort(), "POST", "/api/echo", Token, std::string(AdminSettings::MinRequestBytes + 1, 'a')).Status, 413);
    EXPECT_EQ(Ask(server.GetPort(), "POST", "/api/echo", "", std::string(AdminSettings::MinRequestBytes + 1, 'a')).Status, 401);
}

TEST_F(AdminServerTest, TakesASocketRouteAfterTheListenerOpens)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();
    ASSERT_NE(Upgrade(port, "/api/logs", Token).Status, 101);

    AdminSocketRoute route;
    route.Path = "/api/logs";
    server.AddSocket(route);
    EXPECT_EQ(Upgrade(port, "/api/logs", Token).Status, 101);
    EXPECT_EQ(Upgrade(port, "/api/logs", "").Status, 401);
}

TEST_F(AdminServerTest, CarriesFramesBothWaysOverASocketRoute)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });

    std::atomic<bool> opened{ false };
    std::atomic<bool> closed{ false };
    std::atomic<uint16> closeCode{ 0 };
    std::string remote;
    AdminSocketRoute route;
    route.Path = "/api/socket";
    route.Opened = [&](AdminSocket& socket)
    {
        remote = socket.GetRemoteAddress();
        opened.store(true);
        socket.SendText("welcome");
    };
    route.Received = [](AdminSocket& socket, std::string const& message, bool binary)
    {
        socket.SendText(std::string(binary ? "binary:" : "text:") + message);
    };
    route.Closed = [&](AdminSocket&, std::string const&, uint16 code)
    {
        closeCode.store(code);
        closed.store(true);
    };
    server.AddSocket(std::move(route));

    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;

    SocketClient client;
    ASSERT_TRUE(client.Open(server.GetPort(), "/api/socket", Token));
    std::optional<std::string> const greeting = client.ReadText();
    ASSERT_TRUE(greeting.has_value());
    EXPECT_EQ(*greeting, "welcome");
    EXPECT_TRUE(opened.load());
    EXPECT_EQ(remote, "127.0.0.1");

    ASSERT_TRUE(client.SendText("ping"));
    std::optional<std::string> const echoed = client.ReadText();
    ASSERT_TRUE(echoed.has_value());
    EXPECT_EQ(*echoed, "text:ping");

    ASSERT_TRUE(client.SendClose(1000));
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!closed.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(closed.load());
    EXPECT_EQ(closeCode.load(), 1000);
}

TEST_F(AdminServerTest, ClosesASocketFromItsHandler)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });

    AdminSocketRoute route;
    route.Path = "/api/socket";
    route.Received = [](AdminSocket& socket, std::string const& message, bool)
    {
        if (message == "bye")
            socket.Close("done");
        else
            socket.SendText(message);
    };
    server.AddSocket(std::move(route));

    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;

    SocketClient client;
    ASSERT_TRUE(client.Open(server.GetPort(), "/api/socket", Token));
    ASSERT_TRUE(client.SendText("bye"));

    std::optional<std::pair<uint8, std::string>> const frame = client.ReadFrame();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->first, 0x8);
    ASSERT_GE(frame->second.size(), 2u);
    EXPECT_EQ((static_cast<uint8>(frame->second[0]) << 8) | static_cast<uint8>(frame->second[1]), 1000);
    EXPECT_EQ(frame->second.substr(2), "done");
}

TEST_F(AdminServerTest, ReloadKeepsTheOldListenerWhenTheNewPortIsTaken)
{
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    uint16 const port = server.GetPort();

    asio::io_context context;
    asio::ip::tcp::acceptor taken(context);
    std::error_code code;
    taken.open(asio::ip::tcp::v4(), code);
    ASSERT_FALSE(code);
    taken.bind(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0), code);
    ASSERT_FALSE(code);
    taken.listen(1, code);
    ASSERT_FALSE(code);
    uint16 const held = taken.local_endpoint(code).port();
    ASSERT_FALSE(code);

    AdminSettings moved = Loopback();
    moved.Port = held;
    EXPECT_FALSE(server.Reload(moved));
    EXPECT_TRUE(server.IsRunning());
    EXPECT_EQ(server.GetPort(), port);
    EXPECT_EQ(Get(port, "/api/health", Token).Status, 200);

    taken.close(code);
}

TEST_F(AdminServerTest, StartsBeyondThisMachineWithThePlainHttpOptIn)
{
    std::optional<std::string> const address = Ambrose::GetEnv("AMBROSE_TEST_ADMIN_REMOTE_BIND");
    if (!address || address->empty())
        GTEST_SKIP() << "AMBROSE_TEST_ADMIN_REMOTE_BIND names no address to bind";

    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    AdminServer server = Make();
    server.SetHealthSource([] { return AdminHealth{ "testserver", "", "rev", 0, "running" }; });
    AdminSettings settings = Loopback();
    settings.BindIp = *address;
    settings.AllowPlainHttpRemote = true;

    std::string error;
    ASSERT_TRUE(server.Start(settings, error)) << error;
    std::string const reachable = *address == "0.0.0.0" ? std::string("127.0.0.1") : *address;
    EXPECT_EQ(Ask(server.GetPort(), "GET", "/api/health", "", std::string(), reachable).Status, 401);
    EXPECT_EQ(Ask(server.GetPort(), "GET", "/api/health", Token, std::string(), reachable).Status, 200);

    bool warned = false;
    for (std::string const& line : _harness.Store().Texts("Capture"))
        warned = warned || line.find("Admin.AllowPlainHttpRemote") != std::string::npos;
    EXPECT_TRUE(warned);
}
