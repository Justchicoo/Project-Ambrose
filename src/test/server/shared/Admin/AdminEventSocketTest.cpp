/*
 * Project Ambrose by Imjustchico
 * Tests the admin events feed over real WebSockets on a running app whose world ticks only every five seconds: a setting changed over HTTP reaches a second subscriber, and the first, within one second with its new value, who changed it and why, because a change is announced as it is written rather than at the next tick; a subscriber resuming after a sequence number gets exactly the events after it; the same events read over HTTP after a sequence number; and a socket without the token is refused.
 */

#include "AdminServer.h"
#include "AdminTestClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "MemorySettingStore.h"
#include "ServerApp.h"
#include "Settings.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    class EventApp final : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

        std::shared_ptr<MemorySettingStore> Store = std::make_shared<MemorySettingStore>();

    protected:
        bool OnStart() override { return StartSettings(Store); }
        uint8 GetSettingApps() const override { return SettingApps::Game; }
        std::chrono::milliseconds GetUpdateInterval() const override { return std::chrono::milliseconds(5000); }
        void OnUpdate(std::chrono::milliseconds) override {}
        std::unique_ptr<ConsoleInput> CreateConsoleInput() override { return nullptr; }
    };

    class AdminEventSocketTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSettings.Clear();
            std::filesystem::path const file = _directory.Write("eventserver.conf",
                "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\nConsole.Enable = 0\nWorld.Heartbeat = 60\n"
                "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 0\nAdmin.Token = 0123456789abcdef0123456789abcdef\n");
            _runner = std::thread([this, file] { _exitCode = _app.Run({ "eventserver", "--config", ConfigMgr::PathToUtf8(file) }); });
            auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!_app.IsReady() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ASSERT_TRUE(_app.IsReady());
            ASSERT_NE(_app.GetAdminApi(), nullptr);
            _port = _app.GetAdminApi()->GetPort();
        }

        void TearDown() override
        {
            _app.RequestStop();
            if (_runner.joinable())
                _runner.join();
            sSettings.Clear();
        }

        AdminTest::HttpReply Put(std::string const& key, std::string const& value, std::string const& reason)
        {
            return AdminTest::Ask(_port, "PUT", "/api/settings/" + key, Token, nlohmann::json{ { "value", value }, { "reason", reason } }.dump());
        }

        static std::optional<nlohmann::json> Read(AdminTest::SocketClient& client)
        {
            std::optional<std::string> const text = client.ReadText();
            if (!text)
                return std::nullopt;
            nlohmann::json parsed = nlohmann::json::parse(*text, nullptr, false);
            if (parsed.is_discarded())
                return std::nullopt;
            return parsed;
        }

        static std::vector<nlohmann::json> ReadUntil(AdminTest::SocketClient& client, std::function<bool(nlohmann::json const&)> const& done)
        {
            std::vector<nlohmann::json> messages;
            while (messages.size() < 1000)
            {
                std::optional<nlohmann::json> message = Read(client);
                if (!message)
                    break;
                bool const finished = done(*message);
                messages.push_back(std::move(*message));
                if (finished)
                    break;
            }
            return messages;
        }

        bool Subscribe(AdminTest::SocketClient& client, std::string const& request)
        {
            if (!client.Open(_port, "/api/events", Token) || !client.SendText(request))
                return false;
            std::optional<nlohmann::json> const hello = Read(client);
            return hello && hello->value("type", "") == "hello";
        }

        LogTestDirectory _directory;
        LogTestHarness _harness;
        ConfigMgr _config{ [](std::string const&) { return std::optional<std::string>(); } };
        std::ostringstream _out;
        std::ostringstream _err;
        EventApp _app{ { "eventserver", "eventserver.conf", 0 }, _config, _harness.GetLog(), _out, _err };
        std::thread _runner;
        int _exitCode = -1;
        uint16 _port = 0;
    };

    bool IsChangeTo(nlohmann::json const& message, std::string const& value)
    {
        return message.value("type", "") == "event" && message.value("kind", "") == "setting.changed" && message["data"].value("new", "") == value;
    }
}

TEST_F(AdminEventSocketTest, AChangeReachesASecondDashboardWithinOneSecond)
{
    AdminTest::SocketClient first;
    AdminTest::SocketClient second;
    ASSERT_TRUE(Subscribe(first, R"({"kinds":["setting"]})"));
    ASSERT_TRUE(Subscribe(second, R"({"kinds":["setting"]})"));

    auto const started = std::chrono::steady_clock::now();
    AdminTest::HttpReply const put = Put("World.Heartbeat", "30", "quieter heartbeat");
    ASSERT_EQ(put.Status, 200) << put.Body;
    std::vector<nlohmann::json> const seen = ReadUntil(second, [](nlohmann::json const& message) { return IsChangeTo(message, "30"); });
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    ASSERT_FALSE(seen.empty());
    nlohmann::json const& change = seen.back();
    ASSERT_TRUE(IsChangeTo(change, "30")) << change.dump();
    EXPECT_LT(took.count(), 1000) << "the world ticks every five seconds, so only an announcement made as the change is written arrives in time";
    EXPECT_EQ(change["subject"], "World.Heartbeat");
    EXPECT_EQ(change["data"]["old"], "60");
    EXPECT_EQ(change["data"]["who"], "token");
    EXPECT_EQ(change["data"]["source"], "admin api");
    EXPECT_EQ(change["data"]["reason"], "quieter heartbeat");
    EXPECT_EQ(change["data"]["layer"], "live");

    std::vector<nlohmann::json> const own = ReadUntil(first, [](nlohmann::json const& message) { return IsChangeTo(message, "30"); });
    ASSERT_FALSE(own.empty());
    EXPECT_TRUE(IsChangeTo(own.back(), "30")) << "the dashboard that made the change hears it too";
}

TEST_F(AdminEventSocketTest, AResumeAfterNReceivesExactlyTheEventsAfterN)
{
    for (int value = 31; value <= 35; ++value)
        ASSERT_EQ(Put("World.Heartbeat", std::to_string(value), "stepping").Status, 200);

    nlohmann::json const all = nlohmann::json::parse(AdminTest::Get(_port, "/api/events/after/0", Token).Body, nullptr, false);
    ASSERT_EQ(all["records"].size(), 5u) << all.dump();
    uint64 const second = all["records"][1]["sequence"].get<uint64>();
    uint64 const last = all["records"][4]["sequence"].get<uint64>();

    AdminTest::SocketClient client;
    ASSERT_TRUE(Subscribe(client, nlohmann::json{ { "after", second } }.dump()));
    std::vector<nlohmann::json> const messages = ReadUntil(client, [last](nlohmann::json const& message) { return message.value("sequence", uint64{ 0 }) == last; });
    std::vector<std::string> values;
    for (nlohmann::json const& message : messages)
    {
        EXPECT_NE(message.value("type", ""), "dropped") << message.dump();
        if (message.value("type", "") == "event")
        {
            EXPECT_GT(message["sequence"].get<uint64>(), second);
            values.push_back(message["data"]["new"].get<std::string>());
        }
    }
    EXPECT_EQ(values, (std::vector<std::string>{ "33", "34", "35" }));

    nlohmann::json const rest = nlohmann::json::parse(AdminTest::Get(_port, "/api/events/after/" + std::to_string(second), Token).Body, nullptr, false);
    EXPECT_EQ(rest["records"].size(), 3u) << "the HTTP read after N matches the socket's resume";
    EXPECT_EQ(AdminTest::Get(_port, "/api/events/after/soon", Token).Status, 422);
}

TEST_F(AdminEventSocketTest, ASocketWithoutTheTokenIsRefused)
{
    AdminTest::SocketClient client;
    EXPECT_FALSE(client.Open(_port, "/api/events", ""));
    AdminTest::SocketClient wrong;
    EXPECT_FALSE(wrong.Open(_port, "/api/events", "ffffffffffffffffffffffffffffffff"));
}
