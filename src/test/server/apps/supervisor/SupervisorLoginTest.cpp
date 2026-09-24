/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set and a built login server, runs the real login server under the supervisor and stops it the way the panel does: the stop goes through the app's own admin API and the exit is recorded as one that was asked for, and, with AMBROSE_CLIENT_DIR naming an install so the server has the client's message definitions, a client that finished the session handshake is told the server is shutting down before the process ends, and a restart of the game server beside it leaves that client connected and counted, while a game server something else ends is counted as one crash and started again.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "FakeSessionClient.h"
#include "LoginMessages.h"
#include "LoginTestHarness.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "MySQLConnection.h"
#include "ScopeExit.h"
#include "Supervisor.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    std::filesystem::path Utf8Path(std::string_view text)
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    std::filesystem::path BuiltFolder()
    {
        return Utf8Path(AMBROSE_CHILD_PROCESS_HELPER).parent_path();
    }

    std::filesystem::path Program(std::string const& name)
    {
        std::filesystem::path program = BuiltFolder() / name;
#ifdef _WIN32
        program += ".exe";
#endif
        return program;
    }

    std::filesystem::path LoginServer()
    {
        return Program("loginserver");
    }

    std::filesystem::path GameServer()
    {
        return Program("gameserver");
    }

    std::string Slashes(std::filesystem::path const& path)
    {
        std::string text = ConfigMgr::PathToUtf8(path);
        std::replace(text.begin(), text.end(), '\\', '/');
        return text;
    }

    uint16 FreePort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        uint16 const port = acceptor.local_endpoint().port();
        acceptor.close();
        return port;
    }

    std::optional<MySQLConnectionInfo> TestDatabase(std::string const& name)
    {
        std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
        if (info)
            info->Database = fmt::format("{}_{:08x}", name, std::random_device()());
        return info;
    }

    void DropDatabase(MySQLConnectionInfo info)
    {
        std::string const name = info.Database;
        info.Database.clear();
        MySQLConnection connection(info);
        if (connection.Open() == 0)
            connection.Execute(fmt::format("DROP DATABASE IF EXISTS `{}`", name));
    }
}

namespace
{
    class SupervisedLoginServer
    {
    public:
        SupervisedLoginServer(MySQLConnectionInfo const& login, MySQLConnectionInfo const& characters, bool withClient,
            std::optional<MySQLConnectionInfo> const& world = std::nullopt)
            : Clients(FreePort()), Admin(FreePort())
        {
            std::error_code code;
            std::filesystem::copy_file(BuiltFolder() / "loginserver.conf.dist", _directory.Path() / "loginserver.conf.dist", code);
            std::string const client = withClient ? Ambrose::GetEnv("AMBROSE_CLIENT_DIR").value_or(std::string()) : std::string();
            std::string const dump = withClient ? Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH").value_or(std::string()) : std::string();
            std::filesystem::path const appConfig = _directory.Write("loginserver.conf", fmt::format(
                "BindIP = 127.0.0.1\nLoginServerPort = {}\nClientDir = \"{}\"\nTypeDumpPath = \"{}\"\nSetup.Mode = off\nConsole.Colors = 0\n"
                "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {}\nAdmin.Token = {}\n"
                "LoginDatabaseInfo = \"{}\"\nCharacterDatabaseInfo = \"{}\"\nUpdates.EnableDatabases = 3\nUpdates.AutoSetup = 1\n"
                "Login.ShutdownGrace = 10\n",
                Clients, Slashes(ConfigMgr::PathFromUtf8(client)), Slashes(ConfigMgr::PathFromUtf8(dump)), Admin, Token,
                login.ToConnectionString(), characters.ToConnectionString()));
            std::string apps = "loginserver";
            std::string definitions = fmt::format(
                "App.loginserver.Program = \"{}\"\nApp.loginserver.Config = \"{}\"\n"
                "App.loginserver.StartTimeout = 300\nApp.loginserver.StopTimeout = 60\n",
                Slashes(LoginServer()), Slashes(appConfig));
            if (world)
            {
                std::filesystem::copy_file(BuiltFolder() / "gameserver.conf.dist", _directory.Path() / "gameserver.conf.dist", code);
                std::filesystem::path const realmConfig = _directory.Write("gameserver.conf", fmt::format(
                    "BindIP = 127.0.0.1\nWorldServerPort = {}\nRealmID = 1\nClientDir = \"{}\"\nTypeDumpPath = \"{}\"\nSetup.Mode = off\nConsole.Colors = 0\n"
                    "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {}\nAdmin.Token = {}\n"
                    "LoginDatabaseInfo = \"{}\"\nCharacterDatabaseInfo = \"{}\"\nWorldDatabaseInfo = \"{}\"\nUpdates.EnableDatabases = 7\nUpdates.AutoSetup = 1\n",
                    FreePort(), Slashes(ConfigMgr::PathFromUtf8(client)), Slashes(ConfigMgr::PathFromUtf8(dump)), FreePort(), Token,
                    login.ToConnectionString(), characters.ToConnectionString(), world->ToConnectionString()));
                apps += " gameserver";
                definitions += fmt::format(
                    "App.gameserver.Program = \"{}\"\nApp.gameserver.Config = \"{}\"\n"
                    "App.gameserver.StartTimeout = 300\nApp.gameserver.StopTimeout = 60\n",
                    Slashes(GameServer()), Slashes(realmConfig));
            }
            std::filesystem::path const file = _directory.Write("supervisor.conf", fmt::format(
                "Supervisor.Apps = {}\n{}Supervisor.StateFile = \"{}\"\nSupervisor.OutputDir = \"{}\"\n",
                apps, definitions, Slashes(_directory.Path() / "state.json"), Slashes(_directory.Path() / "output")));
            EXPECT_TRUE(_config.LoadInitial(file).Succeeded());
            _supervisor = std::make_unique<Supervisor>(_harness.GetLog(), ChildBreakSender());
            std::vector<std::string> problems;
            std::string error;
            SupervisorSettings const settings = SupervisorSettings::Load(_config, _directory.Path() / "data", BuiltFolder(), _directory.Path(), problems);
            EXPECT_TRUE(_supervisor->Start(_config, settings, true, problems, error)) << error;
        }

        ~SupervisedLoginServer()
        {
            if (_supervisor)
            {
                for (AppSnapshot const& app : _supervisor->Snapshots())
                    _supervisor->Power(app.Name, PowerAction::Kill, 0);
                std::this_thread::sleep_for(500ms);
                _supervisor->Shutdown();
            }
        }

        SupervisedLoginServer(SupervisedLoginServer const&) = delete;
        SupervisedLoginServer& operator=(SupervisedLoginServer const&) = delete;

        Supervisor& Instance() { return *_supervisor; }

        AppSnapshot App(std::string_view name = "loginserver") const
        {
            for (AppSnapshot const& app : _supervisor->Snapshots())
                if (app.Name == name)
                    return app;
            return AppSnapshot{};
        }

        bool WaitFor(AppState state, std::chrono::seconds timeout, std::string_view name = "loginserver") const
        {
            std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + timeout;
            while (App(name).State != state && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(50ms);
            return App(name).State == state;
        }

        bool WaitForAnotherProcess(std::string_view name, std::optional<int64> before, std::chrono::seconds timeout) const
        {
            std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < deadline)
            {
                AppSnapshot const app = App(name);
                if (app.State == AppState::Running && app.ProcessId && app.ProcessId != before)
                    return true;
                std::this_thread::sleep_for(50ms);
            }
            return false;
        }

        uint64 Sessions() const
        {
            AdminClient const watcher("127.0.0.1", Admin, Token);
            AdminClientResponse const answer = watcher.Send({ "GET", "/api/status", {}, "application/json", {} }, 2s);
            nlohmann::json const body = answer.Answered ? nlohmann::json::parse(answer.Body, nullptr, false) : nlohmann::json();
            return body.is_object() && body["sessions"].is_number_unsigned() ? body["sessions"].get<uint64>() : 0;
        }

        bool WaitForSession(std::chrono::seconds timeout) const
        {
            std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + timeout;
            while (Sessions() == 0 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(50ms);
            return Sessions() != 0;
        }

        bool Said(std::string_view text) const
        {
            for (OutputLine const& line : _supervisor->Output("loginserver", OutputRun::Current, 0))
                if (line.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        uint16 const Clients;
        uint16 const Admin;

    private:
        LogTestHarness _harness;
        LogTestDirectory _directory;
        ConfigMgr _config;
        std::unique_ptr<Supervisor> _supervisor;
    };

    bool HasClientInstall()
    {
        std::optional<std::string> const folder = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        return folder.has_value() && !folder->empty();
    }
}

TEST(SupervisorLoginTest, AStopFromThePanelGoesThroughTheLoginServersAdminApiAndItExitsAsAsked)
{
    std::optional<MySQLConnectionInfo> const login = TestDatabase("ambrose_stop_login");
    std::optional<MySQLConnectionInfo> const characters = TestDatabase("ambrose_stop_characters");
    if (!login || !characters)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::error_code exists;
    if (!std::filesystem::is_regular_file(LoginServer(), exists))
        GTEST_SKIP() << "the login server is not built beside the test helper";
    ScopeExit const drop([&login, &characters] { DropDatabase(*login); DropDatabase(*characters); });

    SupervisedLoginServer server(*login, *characters, false);
    ASSERT_TRUE(server.WaitFor(AppState::Running, 300s)) << server.App().Message;
    EXPECT_TRUE(server.App().AdminEnabled);

    PowerResult const asked = server.Instance().Power("loginserver", PowerAction::Stop, 0);
    ASSERT_TRUE(asked.Accepted) << asked.Message;
    ASSERT_TRUE(server.WaitFor(AppState::Offline, 60s)) << server.App().Message;
    AppSnapshot const stopped = server.App();
    ASSERT_FALSE(stopped.Exits.empty());
    EXPECT_TRUE(stopped.Exits.back().Requested);
    EXPECT_EQ(stopped.Exits.back().Code, std::optional<int64>(0));
    EXPECT_TRUE(server.Said("through its admin API")) << "the stop did not go through the app's admin API";
    EXPECT_TRUE(server.Said("shutting down after the admin API"));
}

TEST(SupervisorLoginClientTest, AStopFromThePanelTellsAConnectedClientBeforeTheLoginServerExits)
{
    std::optional<MySQLConnectionInfo> const login = TestDatabase("ambrose_notice_login");
    std::optional<MySQLConnectionInfo> const characters = TestDatabase("ambrose_notice_characters");
    if (!login || !characters)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    if (!HasClientInstall())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set, and the notice needs the client's message definitions";
    std::error_code exists;
    if (!std::filesystem::is_regular_file(LoginServer(), exists))
        GTEST_SKIP() << "the login server is not built beside the test helper";
    ScopeExit const drop([&login, &characters] { DropDatabase(*login); DropDatabase(*characters); });

    SupervisedLoginServer server(*login, *characters, true);
    ASSERT_TRUE(server.WaitFor(AppState::Running, 300s)) << server.App().Message;

    FakeSessionClient client(server.Clients);
    ASSERT_NE(client.Handshake(), 0);
    ASSERT_TRUE(server.WaitForSession(20s)) << "the login server never counted the client as a session";

    PowerResult const asked = server.Instance().Power("loginserver", PowerAction::Stop, 0);
    ASSERT_TRUE(asked.Accepted) << asked.Message;
    std::optional<DmlMessageData> const notice = LoginTesting::ReadDml(client, 30s);
    ASSERT_TRUE(notice.has_value()) << "the login server closed the connection without telling the client";
    EXPECT_EQ(notice->ServiceId, LoginMessages::LoginService);
    EXPECT_TRUE(client.WaitForClose());
    ASSERT_TRUE(server.WaitFor(AppState::Offline, 60s));
    EXPECT_TRUE(server.App().Exits.back().Requested);
    EXPECT_EQ(server.App().Exits.back().Code, std::optional<int64>(0));
}

TEST(SupervisorLoginClientTest, RestartingTheGameServerLeavesTheLoginServersSessionConnected)
{
    std::optional<MySQLConnectionInfo> const login = TestDatabase("ambrose_realm_login");
    std::optional<MySQLConnectionInfo> const characters = TestDatabase("ambrose_realm_characters");
    std::optional<MySQLConnectionInfo> const world = TestDatabase("ambrose_realm_world");
    if (!login || !characters || !world)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    if (!HasClientInstall())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set, and a game server needs an install";
    std::error_code exists;
    if (!std::filesystem::is_regular_file(LoginServer(), exists) || !std::filesystem::is_regular_file(GameServer(), exists))
        GTEST_SKIP() << "the login and game servers are not built beside the test helper";
    ScopeExit const drop([&login, &characters, &world]
    {
        DropDatabase(*login);
        DropDatabase(*characters);
        DropDatabase(*world);
    });

    SupervisedLoginServer server(*login, *characters, true, world);
    ASSERT_TRUE(server.WaitFor(AppState::Running, 300s)) << server.App().Message;
    ASSERT_TRUE(server.WaitFor(AppState::Running, 300s, "gameserver")) << server.App("gameserver").Message;

    FakeSessionClient client(server.Clients);
    ASSERT_NE(client.Handshake(), 0);
    ASSERT_TRUE(server.WaitForSession(20s)) << "the login server never counted the client as a session";

    std::optional<int64> const before = server.App("gameserver").ProcessId;
    PowerResult const asked = server.Instance().Power("gameserver", PowerAction::Restart, 0);
    ASSERT_TRUE(asked.Accepted) << asked.Message;
    ASSERT_TRUE(server.WaitForAnotherProcess("gameserver", before, 300s)) << server.App("gameserver").Message;

    EXPECT_FALSE(LoginTesting::ReadDml(client, 200ms).has_value()) << "the login server told its client something while the game server restarted";
    EXPECT_FALSE(client.IsClosed());
    EXPECT_EQ(server.Sessions(), 1u);
    EXPECT_EQ(server.App().State, AppState::Running);
    EXPECT_EQ(server.App().Crashes, 0u);
    EXPECT_EQ(server.App("gameserver").Crashes, 0u);
    EXPECT_EQ(server.App("gameserver").Restarts, 1u);
}

TEST(SupervisorLoginClientTest, AGameServerEndedFromOutsideIsOneCrashAndStartsAgain)
{
    std::optional<MySQLConnectionInfo> const login = TestDatabase("ambrose_crash_login");
    std::optional<MySQLConnectionInfo> const characters = TestDatabase("ambrose_crash_characters");
    std::optional<MySQLConnectionInfo> const world = TestDatabase("ambrose_crash_world");
    if (!login || !characters || !world)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    if (!HasClientInstall())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set, and a game server needs an install";
    std::error_code exists;
    if (!std::filesystem::is_regular_file(GameServer(), exists))
        GTEST_SKIP() << "the game server is not built beside the test helper";
    ScopeExit const drop([&login, &characters, &world]
    {
        DropDatabase(*login);
        DropDatabase(*characters);
        DropDatabase(*world);
    });

    SupervisedLoginServer server(*login, *characters, true, world);
    ASSERT_TRUE(server.WaitFor(AppState::Running, 300s, "gameserver")) << server.App("gameserver").Message;
    std::optional<int64> const before = server.App("gameserver").ProcessId;
    ASSERT_TRUE(before.has_value());

    std::optional<ChildProcessIdentity> const identity = ChildProcessHandle::Describe(*before);
    ASSERT_TRUE(identity.has_value());
    std::string error;
    ChildProcessHandle outside = ChildProcessHandle::Adopt(*identity, {}, error);
    ASSERT_TRUE(outside) << error;
    ASSERT_TRUE(outside.EndTree(error)) << error;

    ASSERT_TRUE(server.WaitForAnotherProcess("gameserver", before, 300s)) << server.App("gameserver").Message;
    AppSnapshot const restarted = server.App("gameserver");
    EXPECT_EQ(restarted.Crashes, 1u);
    EXPECT_EQ(restarted.Restarts, 1u);
    ASSERT_FALSE(restarted.Exits.empty());
    EXPECT_FALSE(restarted.Exits.front().Requested);
    EXPECT_EQ(restarted.Exits.front().During, AppState::Running);
}
