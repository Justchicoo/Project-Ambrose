/*
 * Project Ambrose by Imjustchico
 * Tests the log stream over a real WebSocket on a running app: a subscriber filtered to warnings sees only warnings and errors, a reconnecting subscriber gets the backlog, a resume after N gets exactly the records after N, a sensitive console command and a secret setting change stream with their values hidden so a search of every streamed record finds neither, and a bad subscribe request is answered with a problem and a close.
 */

#include "AdminServer.h"
#include "AdminTestClient.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "LogRedaction.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ScriptedPromptInput.h"
#include "ServerApp.h"

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
#include <utility>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";
    constexpr char const* SecretToken = "fedcba9876543210fedcba9876543210";
    constexpr char const* SecretPassword = "hunter2-never-streamed";

    constexpr char const* BaseConfig =
        "LogsDir = logs\nAppender.Console = 1,3,0\nAppender.Stream = 3,1,0,1000\nLogger.root = 3,Console Stream\n"
        "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 0\nAdmin.Token = 0123456789abcdef0123456789abcdef\n";

    class StreamApp final : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

        std::vector<std::string> ConsoleLines;

    protected:
        std::unique_ptr<ConsoleInput> CreateConsoleInput() override
        {
            return std::make_unique<ScriptedPromptInput>(ConsoleLines, false, std::make_shared<ScriptedPromptInput::Counters>());
        }
    };

    class RunningApp
    {
    public:
        RunningApp(LogTestHarness& harness, LogTestDirectory& directory, std::string const& extraConfig = {}, std::vector<std::string> consoleLines = {}, std::function<void(ServerApp&)> const& before = {})
            : _config([](std::string const&) { return std::optional<std::string>(); }), _app({ "testserver", "testserver.conf", 0 }, _config, harness.GetLog(), _out, _err)
        {
            _app.ConsoleLines = std::move(consoleLines);
            if (before)
                before(_app);
            std::filesystem::path const file = directory.Write("testserver.conf", std::string(BaseConfig) + (extraConfig.empty() ? "Console.Enable = 0\n" : extraConfig));
            _runner = std::thread([this, file] { _exitCode = _app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file) }); });
            auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!_app.IsReady() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        ~RunningApp()
        {
            _app.RequestStop();
            if (_runner.joinable())
                _runner.join();
        }

        ServerApp& App() { return _app; }
        uint16 Port() const { return _app.GetAdminApi() ? _app.GetAdminApi()->GetPort() : 0; }

    private:
        ConfigMgr _config;
        std::ostringstream _out;
        std::ostringstream _err;
        StreamApp _app;
        std::thread _runner;
        int _exitCode = -1;
    };

    std::optional<nlohmann::json> ReadMessage(AdminTest::SocketClient& client)
    {
        std::optional<std::string> const text = client.ReadText();
        if (!text)
            return std::nullopt;
        nlohmann::json parsed = nlohmann::json::parse(*text, nullptr, false);
        if (parsed.is_discarded())
            return std::nullopt;
        return parsed;
    }

    std::vector<nlohmann::json> ReadUntil(AdminTest::SocketClient& client, std::function<bool(nlohmann::json const&)> const& done, std::size_t limit = 5000)
    {
        std::vector<nlohmann::json> messages;
        while (messages.size() < limit)
        {
            std::optional<nlohmann::json> message = ReadMessage(client);
            if (!message)
                break;
            bool const finished = done(*message);
            messages.push_back(std::move(*message));
            if (finished)
                break;
        }
        return messages;
    }

    bool IsRecordSaying(nlohmann::json const& message, std::string const& text)
    {
        return message.value("type", "") == "record" && message.value("message", "") == text;
    }

    std::vector<nlohmann::json> Records(std::vector<nlohmann::json> const& messages)
    {
        std::vector<nlohmann::json> out;
        for (nlohmann::json const& message : messages)
            if (message.value("type", "") == "record")
                out.push_back(message);
        return out;
    }

    bool Subscribe(AdminTest::SocketClient& client, uint16 port, std::string const& request)
    {
        if (!client.Open(port, "/api/logs", Token))
            return false;
        if (!client.SendText(request))
            return false;
        std::optional<nlohmann::json> const hello = ReadMessage(client);
        return hello && hello->value("type", "") == "hello";
    }

    class LogStreamSocketTest : public testing::Test
    {
    protected:
        LogTestHarness _harness;
        LogTestDirectory _directory;
    };
}

TEST_F(LogStreamSocketTest, ASubscriberFilteredToWarningsReceivesOnlyWarningsAndErrors)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());

    AdminTest::SocketClient client;
    ASSERT_TRUE(Subscribe(client, running.Port(), "{\"level\":\"warn\"}"));

    Log& log = _harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "quiet info line");
    AMBROSE_LOG(log, LogLevel::Warn, "stream.test", "loud warn line");
    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "another quiet info line");
    AMBROSE_LOG(log, LogLevel::Error, "stream.test", "loud error line");

    std::vector<nlohmann::json> const messages = ReadUntil(client, [](nlohmann::json const& message) { return IsRecordSaying(message, "loud error line"); });
    std::vector<nlohmann::json> const records = Records(messages);
    ASSERT_FALSE(records.empty());
    bool sawWarn = false;
    for (nlohmann::json const& record : records)
    {
        std::string const level = record.value("level", "");
        EXPECT_TRUE(level == "warn" || level == "error" || level == "fatal") << record.dump();
        EXPECT_EQ(record.value("message", "").find("quiet"), std::string::npos) << record.dump();
        sawWarn = sawWarn || record.value("message", "") == "loud warn line";
    }
    EXPECT_TRUE(sawWarn);
    EXPECT_EQ(records.back().value("message", ""), "loud error line");
}

TEST_F(LogStreamSocketTest, AReconnectingSubscriberReceivesTheBacklog)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());

    Log& log = _harness.GetLog();
    for (int line = 1; line <= 5; ++line)
        AMBROSE_LOG(log, LogLevel::Info, "stream.test", "backlog line {}", line);

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        AdminTest::SocketClient client;
        ASSERT_TRUE(Subscribe(client, running.Port(), "{}"));
        std::vector<nlohmann::json> const messages = ReadUntil(client, [](nlohmann::json const& message) { return IsRecordSaying(message, "backlog line 5"); });
        std::vector<std::string> seen;
        for (nlohmann::json const& record : Records(messages))
            if (record.value("message", "").rfind("backlog line", 0) == 0)
                seen.push_back(record.value("message", ""));
        EXPECT_EQ(seen, (std::vector<std::string>{ "backlog line 1", "backlog line 2", "backlog line 3", "backlog line 4", "backlog line 5" })) << "attempt " << attempt;
        ASSERT_TRUE(client.SendClose(1000));
    }
}

TEST_F(LogStreamSocketTest, AResumeAfterNReceivesExactlyTheRecordsAfterN)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());
    Log& log = _harness.GetLog();

    uint64 mark = 0;
    {
        AdminTest::SocketClient first;
        ASSERT_TRUE(Subscribe(first, running.Port(), "{}"));
        AMBROSE_LOG(log, LogLevel::Info, "stream.test", "the mark");
        std::vector<nlohmann::json> const messages = ReadUntil(first, [](nlohmann::json const& message) { return IsRecordSaying(message, "the mark"); });
        ASSERT_FALSE(messages.empty());
        ASSERT_TRUE(IsRecordSaying(messages.back(), "the mark"));
        mark = messages.back()["sequence"].get<uint64>();
        ASSERT_TRUE(first.SendClose(1000));
    }
    ASSERT_NE(mark, 0u);

    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "after one");
    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "after two");
    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "after three");

    AdminTest::SocketClient second;
    ASSERT_TRUE(Subscribe(second, running.Port(), "{\"after\":" + std::to_string(mark) + "}"));
    std::vector<nlohmann::json> const messages = ReadUntil(second, [](nlohmann::json const& message) { return IsRecordSaying(message, "after three"); });
    std::vector<nlohmann::json> const records = Records(messages);
    ASSERT_FALSE(records.empty());
    EXPECT_EQ(records.front()["sequence"].get<uint64>(), mark + 1);
    uint64 previous = mark;
    std::vector<std::string> after;
    for (nlohmann::json const& record : records)
    {
        uint64 const sequence = record["sequence"].get<uint64>();
        EXPECT_GT(sequence, previous) << record.dump();
        previous = sequence;
        if (record.value("message", "").rfind("after ", 0) == 0)
            after.push_back(record.value("message", ""));
    }
    EXPECT_EQ(after, (std::vector<std::string>{ "after one", "after two", "after three" }));
    for (nlohmann::json const& message : messages)
        EXPECT_NE(message.value("type", ""), "dropped") << message.dump();
}

TEST_F(LogStreamSocketTest, ASensitiveCommandAndASecretSettingChangeStreamRedacted)
{
    Log& log = _harness.GetLog();
    auto const registerCommand = [&log](ServerApp& app)
    {
        app.Commands().Register({ "account set password", "<name> <password>", "set a password", true,
            [&log](std::vector<std::string> const&, ConsoleCommandTable::Reply const& reply)
            {
                AMBROSE_LOG(log, LogLevel::Info, "config", "{}", LogRedaction::DescribeSettingChange("Admin.Token", SecretToken, "console"));
                reply("done");
                return true;
            } });
    };
    RunningApp running(_harness, _directory, "Console.Enable = 1\n", { std::string("account set password fred ") + SecretPassword }, registerCommand);
    ASSERT_TRUE(running.App().IsReady());

    AdminTest::SocketClient client;
    ASSERT_TRUE(Subscribe(client, running.Port(), "{}"));
    AMBROSE_LOG(log, LogLevel::Info, "stream.test", "the end of the redaction check");

    bool sawCommand = false;
    bool sawSetting = false;
    std::vector<nlohmann::json> const messages = ReadUntil(client, [](nlohmann::json const& message) { return IsRecordSaying(message, "the end of the redaction check"); });
    for (nlohmann::json const& record : Records(messages))
    {
        std::string const text = record.value("message", "");
        EXPECT_EQ(text.find(SecretPassword), std::string::npos) << text;
        EXPECT_EQ(text.find(SecretToken), std::string::npos) << text;
        sawCommand = sawCommand || text == "Console: account set password (arguments hidden)";
        sawSetting = sawSetting || text == "Setting Admin.Token changed to *** from console";
    }
    EXPECT_TRUE(sawCommand);
    EXPECT_TRUE(sawSetting);
}

TEST_F(LogStreamSocketTest, ABadSubscribeRequestIsAnsweredWithAProblemAndAClose)
{
    RunningApp running(_harness, _directory);
    ASSERT_TRUE(running.App().IsReady());

    AdminTest::SocketClient client;
    ASSERT_TRUE(client.Open(running.Port(), "/api/logs", Token));
    ASSERT_TRUE(client.SendText("{\"level\":\"loud\"}"));
    std::optional<std::pair<uint8, std::string>> const frame = client.ReadFrame();
    ASSERT_TRUE(frame.has_value());
    ASSERT_EQ(frame->first, 0x1) << frame->second;
    nlohmann::json const problem = nlohmann::json::parse(frame->second, nullptr, false);
    ASSERT_FALSE(problem.is_discarded()) << frame->second;
    EXPECT_EQ(problem.value("type", ""), "problem");
    EXPECT_EQ(problem.value("code", ""), "bad_request");
    EXPECT_NE(problem.value("message", "").find("level"), std::string::npos) << problem.dump();
    std::optional<std::pair<uint8, std::string>> const next = client.ReadFrame();
    EXPECT_TRUE(!next || next->first == 0x8);
}
