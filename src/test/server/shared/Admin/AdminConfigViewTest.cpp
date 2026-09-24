/*
 * Project Ambrose by Imjustchico
 * Tests the settings answer and the shutdown route: every loaded key with its effective value, shipped default, layer, file and line, secrets masked in both values, restart reasons only on the keys an app declares, one by name and one for every key under a prefix, and POST /api/shutdown refusing bad bodies field by field, scheduling and cancelling a countdown, and stopping the app, and that a secret stays masked until the caller holds the permission to read one, which the route asks per request rather than once when it was registered.
 */

#include "AdminAuth.h"
#include "AdminConfigView.h"
#include "AdminRouter.h"
#include "AdminServer.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ServerApp.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";
    std::string const Header = "# Project Ambrose by Imjustchico\n# Test configuration.\n";

    struct HttpReply
    {
        int Status = 0;
        std::string Body;
    };

    HttpReply Call(uint16 port, std::string const& method, std::string const& path, std::string const& body = {})
    {
        asio::io_context context;
        asio::ip::tcp::socket socket(context);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
        std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\nAuthorization: Bearer " + Token + "\r\n";
        if (!body.empty())
            request += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
        request += "\r\n" + body;
        asio::write(socket, asio::buffer(request));
        std::string raw;
        std::error_code error;
        std::array<char, 4096> chunk{};
        while (true)
        {
            std::size_t const got = socket.read_some(asio::buffer(chunk), error);
            if (error)
                break;
            raw.append(chunk.data(), got);
        }
        HttpReply reply;
        std::size_t const split = raw.find("\r\n\r\n");
        std::string const head = split == std::string::npos ? raw : raw.substr(0, split);
        reply.Body = split == std::string::npos ? std::string() : raw.substr(split + 4);
        std::size_t const space = head.find(' ');
        if (space != std::string::npos)
            reply.Status = std::atoi(head.c_str() + space + 1);
        return reply;
    }

    bool WaitFor(std::function<bool()> const& condition, std::chrono::milliseconds timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (!condition())
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    std::map<std::string, nlohmann::json> ByKey(nlohmann::json const& body)
    {
        std::map<std::string, nlohmann::json> settings;
        for (nlohmann::json const& entry : body.at("settings"))
            settings.emplace(entry.at("key").get<std::string>(), entry);
        return settings;
    }

    bool HasField(nlohmann::json const& problem, std::string const& field)
    {
        return problem.contains("fields") && problem["fields"].is_object() && problem["fields"].contains(field);
    }

    class DeclaringApp : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

    protected:
        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            return { { "Console.Enable", "the console reader starts with the app" } };
        }
    };

    class RunningApp
    {
    public:
        RunningApp(LogTestHarness& harness, LogTestDirectory& directory)
            : _config([](std::string const&) { return std::optional<std::string>(); }), _app({ "testserver", "testserver.conf", 0 }, _config, harness.GetLog(), _out, _err)
        {
            std::filesystem::path const file = directory.Write("testserver.conf",
                "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\nConsole.Enable = 0\n"
                "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 0\nAdmin.Token = 0123456789abcdef0123456789abcdef\n");
            _runner = std::thread([this, file] { _exitCode = _app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file) }); _finished = true; });
            WaitFor([this] { return _app.IsReady(); }, std::chrono::seconds(10));
        }

        ~RunningApp()
        {
            _app.RequestStop();
            if (_runner.joinable())
                _runner.join();
        }

        uint16 Port() const { return _app.GetAdminApi() ? _app.GetAdminApi()->GetPort() : 0; }
        bool Finished() const { return _finished.load(); }
        int ExitCode() const { return _exitCode; }

    private:
        ConfigMgr _config;
        std::ostringstream _out;
        std::ostringstream _err;
        DeclaringApp _app;
        std::thread _runner;
        std::atomic<bool> _finished{ false };
        int _exitCode = -1;
    };
}

TEST(AdminConfigViewTest, EachKeyShowsItsValueDefaultLayerAndSourceWithSecretsMasked)
{
    LogTestDirectory directory;
    directory.Write("testserver.conf.dist", Header +
        "Login.Name = Ambrose\nWorld.UpdateInterval = 50\nAdmin.Token = 0123456789abcdef0123456789abcdef\nLoginDatabaseInfo = 127.0.0.1;3306;ambrose;distpass;ambrose_login\n");
    std::filesystem::path const file = directory.Write("testserver.conf", Header +
        "Login.Name = Local\nLoginDatabaseInfo = 127.0.0.1;3306;ambrose;localpass;ambrose_login\nExtra.Key = 1\n");
    ConfigMgr config([](std::string const& name) { return name == "AMBROSE_WORLD_UPDATE_INTERVAL" ? std::optional<std::string>("100") : std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file, {}, { { "Admin.Token", "fedcba9876543210fedcba9876543210" } }).Succeeded());

    std::array<RestartRequiredOption, 2> const restart{ { { "World.UpdateInterval", "the tick starts with the app" }, { "Extra.*", "everything extra is read once" } } };
    std::string const text = AdminConfigView::SettingsJson(config, restart, false);
    EXPECT_EQ(text.find("distpass"), std::string::npos);
    EXPECT_EQ(text.find("localpass"), std::string::npos);
    EXPECT_EQ(text.find("0123456789abcdef"), std::string::npos);
    EXPECT_EQ(text.find("fedcba9876543210"), std::string::npos);

    nlohmann::json const body = nlohmann::json::parse(text);
    EXPECT_EQ(body["schema"], AdminConfigView::SchemaVersion);
    EXPECT_EQ(std::filesystem::path(ConfigMgr::PathFromUtf8(body["file"].get<std::string>())).filename(), "testserver.conf");
    std::map<std::string, nlohmann::json> const settings = ByKey(body);

    nlohmann::json const& name = settings.at("Login.Name");
    EXPECT_EQ(name["value"], "Local");
    EXPECT_EQ(name["layer"], "config");
    EXPECT_EQ(ConfigMgr::PathFromUtf8(name["file"].get<std::string>()).filename(), "testserver.conf");
    EXPECT_EQ(name["line"], 3);
    EXPECT_EQ(name["default"], "Ambrose");
    EXPECT_EQ(ConfigMgr::PathFromUtf8(name["default_file"].get<std::string>()).filename(), "testserver.conf.dist");
    EXPECT_EQ(name["secret"], false);
    EXPECT_TRUE(name["restart_reason"].is_null());

    nlohmann::json const& interval = settings.at("World.UpdateInterval");
    EXPECT_EQ(interval["value"], "100");
    EXPECT_EQ(interval["layer"], "environment");
    EXPECT_EQ(interval["file"], "AMBROSE_WORLD_UPDATE_INTERVAL");
    EXPECT_EQ(interval["default"], "50");
    EXPECT_EQ(interval["restart_reason"], "the tick starts with the app");

    nlohmann::json const& database = settings.at("LoginDatabaseInfo");
    EXPECT_EQ(database["value"], "127.0.0.1;3306;ambrose;***;ambrose_login");
    EXPECT_EQ(database["default"], "127.0.0.1;3306;ambrose;***;ambrose_login");
    EXPECT_EQ(database["secret"], true);

    nlohmann::json const& token = settings.at("Admin.Token");
    EXPECT_EQ(token["layer"], "override");
    EXPECT_EQ(token["value"], "***");
    EXPECT_EQ(token["default"], "***");

    nlohmann::json const& extra = settings.at("Extra.Key");
    EXPECT_EQ(extra["restart_reason"], "everything extra is read once");
    EXPECT_TRUE(extra["default"].is_null());
    EXPECT_TRUE(extra["default_file"].is_null());
}

TEST(AdminConfigViewTest, ARunningAppMarksOnlyTheOptionsItDeclaresRestartRequired)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    RunningApp running(harness, directory);
    ASSERT_NE(running.Port(), 0);
    HttpReply const reply = Call(running.Port(), "GET", "/api/settings");
    ASSERT_EQ(reply.Status, 200) << reply.Body;
    std::map<std::string, nlohmann::json> const settings = ByKey(nlohmann::json::parse(reply.Body));
    EXPECT_EQ(settings.at("Console.Enable")["restart_reason"], "the console reader starts with the app");
    EXPECT_TRUE(settings.at("LogsDir")["restart_reason"].is_null());
    EXPECT_EQ(settings.at("Admin.Token")["value"], "***");
}

TEST(AdminConfigViewTest, ShutdownRefusesBadBodiesSchedulesCancelsAndStops)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    RunningApp running(harness, directory);
    uint16 const port = running.Port();
    ASSERT_NE(port, 0);

    HttpReply const negative = Call(port, "POST", "/api/shutdown", "{\"seconds\":-1}");
    EXPECT_EQ(negative.Status, 422);
    EXPECT_TRUE(HasField(nlohmann::json::parse(negative.Body), "seconds")) << negative.Body;
    HttpReply const tooLong = Call(port, "POST", "/api/shutdown", "{\"seconds\":86401}");
    EXPECT_EQ(tooLong.Status, 422);
    HttpReply const unknown = Call(port, "POST", "/api/shutdown", "{\"bogus\":1}");
    EXPECT_EQ(unknown.Status, 422);
    EXPECT_TRUE(HasField(nlohmann::json::parse(unknown.Body), "bogus")) << unknown.Body;
    HttpReply const both = Call(port, "POST", "/api/shutdown", "{\"cancel\":true,\"seconds\":5}");
    EXPECT_EQ(both.Status, 422);
    EXPECT_TRUE(HasField(nlohmann::json::parse(both.Body), "cancel")) << both.Body;
    EXPECT_EQ(Call(port, "POST", "/api/shutdown", "[1]").Status, 422);

    HttpReply const nothing = Call(port, "POST", "/api/shutdown", "{\"cancel\":true}");
    ASSERT_EQ(nothing.Status, 200) << nothing.Body;
    EXPECT_EQ(nlohmann::json::parse(nothing.Body)["cancelled"], false);

    HttpReply const later = Call(port, "POST", "/api/shutdown", "{\"seconds\":600}");
    ASSERT_EQ(later.Status, 202) << later.Body;
    EXPECT_EQ(nlohmann::json::parse(later.Body)["stopping_in"], 600);
    HttpReply const cancelled = Call(port, "POST", "/api/shutdown", "{\"cancel\":true}");
    ASSERT_EQ(cancelled.Status, 200) << cancelled.Body;
    EXPECT_EQ(nlohmann::json::parse(cancelled.Body)["cancelled"], true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(running.Finished());

    HttpReply const now = Call(port, "POST", "/api/shutdown", "{}");
    ASSERT_EQ(now.Status, 202) << now.Body;
    EXPECT_EQ(nlohmann::json::parse(now.Body)["stopping_in"], 0);
    EXPECT_TRUE(WaitFor([&running] { return running.Finished(); }, std::chrono::seconds(10)));
    EXPECT_EQ(running.ExitCode(), 0);
    EXPECT_NE(harness.Device().Output().find("The admin API asked testserver to stop now"), std::string::npos);
}

TEST(AdminConfigViewTest, ASecretIsMaskedUntilTheCallerMayReadOneAndTheRouteAsksPerRequest)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("testserver.conf", Header +
        "Login.Name = Local\nAdmin.Token = 0123456789abcdef0123456789abcdef\n");
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    std::string const masked = AdminConfigView::SettingsJson(config, {}, false);
    EXPECT_EQ(masked.find("0123456789abcdef"), std::string::npos);
    std::string const revealed = AdminConfigView::SettingsJson(config, {}, true);
    EXPECT_NE(revealed.find("0123456789abcdef"), std::string::npos);
    EXPECT_NE(revealed.find("Local"), std::string::npos) << "revealing a secret changes nothing about the rest";

    AdminAuth auth(10, 1.0);
    auth.SetToken("0123456789abcdef0123456789abcdef");
    AdminRouter routes(auth);
    AdminConfigView::Register(routes, config);

    std::vector<std::string> asked;
    bool allow = false;
    routes.SetPermissionCheck([&asked, &allow](AdminRequest const&, std::string_view permission)
    {
        asked.emplace_back(permission);
        if (permission == "settings.read")
            return PermissionVerdict::Allowed;
        return allow ? PermissionVerdict::Allowed : PermissionVerdict::Forbidden;
    });

    AdminRequest request;
    request.Method = "GET";
    request.Path = "/api/settings";
    request.RemoteAddress = "127.0.0.1";
    request.Authorization = "Bearer 0123456789abcdef0123456789abcdef";

    AdminResponse const without = routes.Dispatch(request);
    EXPECT_EQ(without.Status, 200);
    EXPECT_EQ(without.Body.find("0123456789abcdef"), std::string::npos);

    allow = true;
    AdminResponse const with = routes.Dispatch(request);
    EXPECT_EQ(with.Status, 200);
    EXPECT_NE(with.Body.find("0123456789abcdef"), std::string::npos);

    EXPECT_EQ(std::count(asked.begin(), asked.end(), std::string("settings.secrets.read")), 2)
        << "the reveal is decided once per request, so it can be recorded each time it happens";
}
