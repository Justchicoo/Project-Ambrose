/*
 * Project Ambrose by Imjustchico
 * Tests the status, apps, capabilities and errors routes: two loopback clients show as two sessions and none within a second of closing, reported memory sits within ten percent of the operating system's own figure, the status, apps and capabilities fields are only ever added, a missing type dump appears as a problem and clears once a dump is in use, the capabilities response lists every entry the registries hold, and an app's own apps answer is exactly itself in the shape the supervisor will share.
 */

#include "AdminCapabilities.h"
#include "AdminServer.h"
#include "ListenerSettings.h"
#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "NetworkSettings.h"
#include "ProcessInfo.h"
#include "ServerApp.h"
#include "Socket.h"
#include "SocketMgr.h"
#include "StatsRegistry.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    struct HttpReply
    {
        int Status = 0;
        std::string Body;
    };

    HttpReply Get(uint16 port, std::string const& path, std::string const& token)
    {
        asio::io_context context;
        asio::ip::tcp::socket socket(context);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
        std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
        if (!token.empty())
            request += "Authorization: Bearer " + token + "\r\n";
        request += "\r\n";
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

    std::set<std::string> Keys(nlohmann::json const& object)
    {
        std::set<std::string> keys;
        for (auto const& [key, value] : object.items())
            keys.insert(key);
        return keys;
    }

    uint64 OperatingSystemResidentBytes()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb))
            return 0;
        return static_cast<uint64>(counters.WorkingSetSize);
#else
        std::ifstream statm("/proc/self/statm");
        uint64 pages = 0;
        uint64 resident = 0;
        statm >> pages >> resident;
        return resident * static_cast<uint64>(sysconf(_SC_PAGESIZE));
#endif
    }

    class PlainSocket : public Socket
    {
    public:
        using Socket::Socket;

    protected:
        void OnFrame(Frame&) override
        {
        }
    };

    class AdminStatusTest : public testing::Test
    {
    protected:
        void TearDown() override
        {
            sStats.Unpublish("sessions");
        }

        ListenerSettings Loopback() const
        {
            ListenerSettings settings;
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

        AdminStatusSnapshot Snapshot() const
        {
            AdminStatusSnapshot snapshot;
            snapshot.App.Name = "testserver";
            snapshot.App.Role = "testserver";
            snapshot.State = "running";
            snapshot.Process = Ambrose::ProcessInfo::Snapshot();
            if (std::optional<Ambrose::StatValue> const sessions = sStats.Get("sessions"))
                if (int64 const* const count = std::get_if<int64>(&*sessions))
                    snapshot.Sessions = static_cast<uint64>(*count);
            snapshot.Stats = sStats.Collect();
            return snapshot;
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
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
            _runner = std::thread([this, file] { _exitCode = _app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file) }); });
            WaitFor([this] { return _app.IsReady(); }, std::chrono::seconds(10));
        }

        ~RunningApp()
        {
            _app.RequestStop();
            if (_runner.joinable())
                _runner.join();
        }

        ServerApp& App() { return _app; }
        uint16 Port() const { return _app.GetAdminApi() ? _app.GetAdminApi()->GetPort() : 0; }
        int ExitCode() const { return _exitCode; }

    private:
        ConfigMgr _config;
        std::ostringstream _out;
        std::ostringstream _err;
        ServerApp _app;
        std::thread _runner;
        int _exitCode = -1;
    };
}

TEST_F(AdminStatusTest, TwoLoopbackClientsShowAsTwoSessionsAndNoneWithinASecondOfClosing)
{
    SocketMgr<PlainSocket> manager;
    NetworkSettings network;
    network.BindIp = "127.0.0.1";
    network.Port = 0;
    network.Threads = 1;
    std::string error;
    ASSERT_TRUE(manager.StartNetwork(network, error)) << error;
    uint16 const listener = manager.GetPort();
    ASSERT_NE(listener, 0);
    sStats.Publish("sessions", [&manager] { return Ambrose::StatValue(static_cast<int64>(manager.GetConnectionCount())); });

    AdminServer server = Make();
    AdminStatus::Register(server.Routes(), [this] { return Snapshot(); });
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;

    asio::io_context context;
    asio::ip::tcp::socket first(context);
    asio::ip::tcp::socket second(context);
    first.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), listener));
    second.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), listener));
    ASSERT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 2; }, std::chrono::seconds(5)));

    HttpReply const two = Get(server.GetPort(), "/api/status", Token);
    ASSERT_EQ(two.Status, 200);
    EXPECT_EQ(nlohmann::json::parse(two.Body)["sessions"], 2);

    first.close();
    second.close();
    auto const started = std::chrono::steady_clock::now();
    bool cleared = false;
    while (std::chrono::steady_clock::now() - started < std::chrono::seconds(1))
    {
        HttpReply const now = Get(server.GetPort(), "/api/status", Token);
        if (now.Status == 200 && nlohmann::json::parse(now.Body)["sessions"] == 0)
        {
            cleared = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(cleared);

    server.Stop();
    manager.StopNetwork();
}

TEST_F(AdminStatusTest, ReportedMemoryIsWithinTenPercentOfTheOperatingSystemsFigure)
{
    std::optional<Ambrose::ProcessSnapshot> const snapshot = Ambrose::ProcessInfo::Snapshot();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_GT(snapshot->ThreadCount, 0u);
    uint64 const reported = snapshot->ResidentBytes;
    uint64 const system = OperatingSystemResidentBytes();
    ASSERT_GT(system, 0u);
    uint64 const difference = reported > system ? reported - system : system - reported;
    EXPECT_LE(difference * 10, system) << "reported " << reported << " against " << system;

    AdminStatusSnapshot status = Snapshot();
    status.Process = snapshot;
    nlohmann::json const parsed = nlohmann::json::parse(AdminStatus::StatusJson(status));
    EXPECT_EQ(parsed["memory"]["resident_bytes"], reported);
    EXPECT_EQ(parsed["threads"], snapshot->ThreadCount);
}

TEST_F(AdminStatusTest, FieldsAreOnlyEverAdded)
{
    std::vector<std::string> const statusVersionOne{ "schema", "app", "role", "realm", "revision", "state", "uptime", "memory", "threads", "sessions", "tick", "stats", "problems" };
    std::vector<std::string> const appsVersionOne{ "name", "role", "realm", "address", "port", "revision" };
    std::vector<std::string> const capabilitiesVersionOne{ "schema", "reload_targets", "schedule_actions", "announcement_channels", "problem_codes" };

    AdminStatusSnapshot const snapshot = Snapshot();
    std::set<std::string> const status = Keys(nlohmann::json::parse(AdminStatus::StatusJson(snapshot)));
    std::set<std::string> const app = Keys(nlohmann::json::parse(AdminStatus::AppsJson(snapshot))[0]);
    std::set<std::string> const capabilities = Keys(nlohmann::json::parse(AdminStatus::CapabilitiesJson()));

    for (std::string const& field : statusVersionOne)
    {
        EXPECT_TRUE(status.contains(field)) << "status lost " << field;
        EXPECT_NE(std::find(AdminStatus::StatusFields().begin(), AdminStatus::StatusFields().end(), field), AdminStatus::StatusFields().end()) << field;
    }
    for (std::string const& field : appsVersionOne)
    {
        EXPECT_TRUE(app.contains(field)) << "apps lost " << field;
        EXPECT_NE(std::find(AdminStatus::AppFields().begin(), AdminStatus::AppFields().end(), field), AdminStatus::AppFields().end()) << field;
    }
    for (std::string const& field : capabilitiesVersionOne)
    {
        EXPECT_TRUE(capabilities.contains(field)) << "capabilities lost " << field;
        EXPECT_NE(std::find(AdminStatus::CapabilityFields().begin(), AdminStatus::CapabilityFields().end(), field), AdminStatus::CapabilityFields().end()) << field;
    }
    for (std::string const& field : AdminStatus::StatusFields())
        EXPECT_TRUE(status.contains(field)) << "status declares " << field << " and does not write it";
    EXPECT_EQ(nlohmann::json::parse(AdminStatus::StatusJson(snapshot))["schema"], AdminStatus::SchemaVersion);
}

TEST_F(AdminStatusTest, TheErrorsRouteReportsAGroupPerPlaceAnErrorWasRaised)
{
    sLog.GetErrors().Clear();
    for (char const* why : { "a password", "again" })
        LOG_ERROR("server.test", "could not reach {} for {}", "the store", why);

    nlohmann::json const body = nlohmann::json::parse(AdminStatus::ErrorsJson("gameserver"));
    EXPECT_EQ(body["schema"], AdminStatus::SchemaVersion);
    ASSERT_EQ(body["groups"].size(), 1u) << body.dump();
    nlohmann::json const& group = body["groups"][0];
    EXPECT_EQ(group["app"], "gameserver");
    EXPECT_EQ(group["category"], "server.test");
    EXPECT_EQ(group["level"], "error");
    EXPECT_EQ(group["count"], 2u);
    EXPECT_EQ(group["template"], "could not reach {} for {}");
    EXPECT_NE(std::string(group["file"]).find("AdminStatusTest.cpp"), std::string::npos);
    EXPECT_GT(group["line"].get<uint32>(), 0u);
    EXPECT_LE(group["first_epoch_ms"].get<int64>(), group["last_epoch_ms"].get<int64>());

    std::set<std::string> const keys = Keys(group);
    for (std::string const& field : AdminStatus::ErrorFields())
        EXPECT_TRUE(keys.contains(field)) << "errors declares " << field << " and does not write it";
    sLog.GetErrors().Clear();
}

TEST_F(AdminStatusTest, AMissingTypeDumpIsAProblemUntilOneIsInUse)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());
    ASSERT_NE(running.Port(), 0);

    running.App().SetClientSetup(true, false, false, "No type dump was built for this revision");
    HttpReply const missing = Get(running.Port(), "/api/status", Token);
    ASSERT_EQ(missing.Status, 200);
    nlohmann::json const before = nlohmann::json::parse(missing.Body)["problems"];
    ASSERT_EQ(before.size(), 1u);
    EXPECT_EQ(before[0]["code"], "type_dump_missing");
    EXPECT_EQ(before[0]["subject"], "type dump");
    EXPECT_EQ(before[0]["message"], "No type dump was built for this revision");
    EXPECT_TRUE(sAdminCapabilities.HasProblemCode("type_dump_missing"));

    running.App().SetClientSetup(true, true, false, "");
    HttpReply const rebuilt = Get(running.Port(), "/api/status", Token);
    ASSERT_EQ(rebuilt.Status, 200);
    EXPECT_TRUE(nlohmann::json::parse(rebuilt.Body)["problems"].empty());

    running.App().SetClientSetup(true, true, true, "");
    EXPECT_EQ(nlohmann::json::parse(Get(running.Port(), "/api/status", Token).Body)["problems"][0]["code"], "type_dump_stale");
    running.App().SetClientSetup(false, false, false, "");
    EXPECT_EQ(nlohmann::json::parse(Get(running.Port(), "/api/status", Token).Body)["problems"][0]["code"], "install_missing");
}

TEST_F(AdminStatusTest, CapabilitiesListEveryEntryTheRegistriesHold)
{
    sAdminCapabilities.RegisterStandardProblems();
    sAdminCapabilities.AddReloadTarget("zz-test-target");
    sAdminCapabilities.AddScheduleAction("zz-test-action");
    sAdminCapabilities.AddAnnouncementChannel("zz-test-channel");
    sAdminCapabilities.AddProblemCode("zz_test_problem", "A code added by the test");

    AdminServer server = Make();
    AdminStatus::Register(server.Routes(), [this] { return Snapshot(); });
    std::string error;
    ASSERT_TRUE(server.Start(Loopback(), error)) << error;
    HttpReply const reply = Get(server.GetPort(), "/api/capabilities", Token);
    ASSERT_EQ(reply.Status, 200);
    nlohmann::json const body = nlohmann::json::parse(reply.Body);

    EXPECT_EQ(body["reload_targets"].get<std::vector<std::string>>(), sAdminCapabilities.ReloadTargets());
    EXPECT_EQ(body["schedule_actions"].get<std::vector<std::string>>(), sAdminCapabilities.ScheduleActions());
    EXPECT_EQ(body["announcement_channels"].get<std::vector<std::string>>(), sAdminCapabilities.AnnouncementChannels());
    std::vector<std::pair<std::string, std::string>> codes;
    for (nlohmann::json const& entry : body["problem_codes"])
        codes.emplace_back(entry["code"].get<std::string>(), entry["description"].get<std::string>());
    EXPECT_EQ(codes, sAdminCapabilities.ProblemCodes());
    EXPECT_GE(codes.size(), 7u);
    server.Stop();
}

TEST_F(AdminStatusTest, AnAppAnswersTheAppsListWithExactlyItself)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());
    running.App().SetListener("127.0.0.1", 12000);

    HttpReply const reply = Get(running.Port(), "/api/apps", Token);
    ASSERT_EQ(reply.Status, 200);
    nlohmann::json const body = nlohmann::json::parse(reply.Body);
    ASSERT_TRUE(body.is_array());
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(body[0]["name"], "testserver");
    EXPECT_EQ(body[0]["role"], "testserver");
    EXPECT_EQ(body[0]["address"], "127.0.0.1");
    EXPECT_EQ(body[0]["port"], 12000);
    std::set<std::string> const expected(AdminStatus::AppFields().begin(), AdminStatus::AppFields().end());
    EXPECT_EQ(Keys(body[0]), expected);

    HttpReply const status = Get(running.Port(), "/api/status", Token);
    ASSERT_EQ(status.Status, 200);
    nlohmann::json const parsed = nlohmann::json::parse(status.Body);
    EXPECT_EQ(parsed["app"], "testserver");
    EXPECT_EQ(parsed["state"], "running");
    EXPECT_TRUE(parsed["tick"].is_null());
}
