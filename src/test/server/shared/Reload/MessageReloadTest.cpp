/*
 * Project Ambrose by Imjustchico
 * Checks the reload an operator runs after the client's message definitions change, through the target a running app registers once it names the install it reads them from: the install is a Root.wad the test builds, a definition broken on disk refuses the reload with its error and leaves the generation that was serving, whose declared messages still encode, and once the file is mended the same reload takes a new generation; run over the admin API, two broken definitions answer 409 naming both, the listing and the event feed report the same failure, and the old generation goes on serving.
 */

#include "AdminServer.h"
#include "AdminTestClient.h"
#include "BaseMessageFixtures.h"
#include "ByteBuffer.h"
#include "ConfigMgr.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "MessageHandlerTable.h"
#include "MessageRegistry.h"
#include "ReloadMgr.h"
#include "ScopeExit.h"
#include "ServerApp.h"
#include "SessionBase.h"
#include "SystemMessageRules.h"
#include "SystemMessages.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    class BaseTable : public MessageHandlerTable<SessionBase>
    {
    public:
        BaseTable() : MessageHandlerTable<SessionBase>("reloadtest", { SystemMessages::SystemService, SystemMessages::ExtendedBaseService })
        {
            SystemMessages::AddRules(*this);
        }
    };

    class MessageApp : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

        std::filesystem::path Install;
        bool Loaded = false;

    protected:
        bool OnStart() override
        {
            Loaded = sMessageRegistry.LoadFromClient(Install);
            if (Loaded)
                SetMessageSource(Install);
            return Loaded;
        }

        std::unique_ptr<ConsoleInput> CreateConsoleInput() override { return nullptr; }
        std::chrono::milliseconds GetUpdateInterval() const override { return std::chrono::milliseconds(20); }
        void OnUpdate(std::chrono::milliseconds) override {}
        void OnStop() override {}
    };

    std::string_view const BrokenExtendedBase = R"(<?xml version="1.0" ?>
<FixtureExtendedBaseMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">2</ServiceID><ProtocolType TYPE="STR">EXTENDEDBASE</ProtocolType></RECORD></_ProtocolInfo>
<MSG_SERVERMESSAGE><RECORD><Modal TYPE="NOTATYPE"></Modal><Message TYPE="WSTR"></Message></RECORD></MSG_SERVERMESSAGE>
</FixtureExtendedBaseMessages>
)";

    std::string_view const TwoBrokenExtendedBase = R"(<?xml version="1.0" ?>
<FixtureExtendedBaseMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">2</ServiceID><ProtocolType TYPE="STR">EXTENDEDBASE</ProtocolType></RECORD></_ProtocolInfo>
<MSG_RAW_TEXT><RECORD><Message TYPE="STR"></Message></RECORD></MSG_RAW_TEXT>
<MSG_SERVERMESSAGE><RECORD><Modal TYPE="NOTATYPE"></Modal><Message TYPE="WSTR"></Message></RECORD></MSG_SERVERMESSAGE>
<MSG_FORCE_DISCONNECT><RECORD><Type TYPE="ALSONOTATYPE"></Type><TimeStamp TYPE="STR"></TimeStamp><Message TYPE="STR"></Message></RECORD></MSG_FORCE_DISCONNECT>
</FixtureExtendedBaseMessages>
)";

    constexpr char const* AdminToken = "0123456789abcdef0123456789abcdef";

    bool NamesBoth(nlohmann::json const& errors)
    {
        bool first = false;
        bool second = false;
        for (nlohmann::json const& error : errors)
        {
            std::string const text = error.get<std::string>();
            first = first || (text.find("NOTATYPE") != std::string::npos && text.find("ALSONOTATYPE") == std::string::npos);
            second = second || text.find("ALSONOTATYPE") != std::string::npos;
        }
        return first && second;
    }

    class MessageReloadTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            ASSERT_TRUE(_table.Declare(sMessageRegistry, errors)) << (errors.empty() ? std::string() : errors.front());
            _install = _directory.Path() / "install";
            std::filesystem::create_directories(_install / "Data" / "GameData");
            WriteRoot(BaseMessageFixtures::ExtendedBaseXml);
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            sMessageRegistry.Clear();
        }

        void WriteRoot(std::string_view extendedBase)
        {
            KiwadBuilder builder(2);
            builder.Add("FixtureSystemMessages.xml", BaseMessageFixtures::SystemXml, false);
            builder.Add("FixtureExtendedBaseMessages.xml", extendedBase, true);
            std::vector<uint8> const archive = builder.Build();
            std::ofstream(_install / "Data" / "GameData" / "Root.wad", std::ios::binary | std::ios::trunc)
                .write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
        }

        std::filesystem::path WriteConfig(std::string const& extra = {})
        {
            std::filesystem::path const file = _directory.Path() / "reloadserver.conf";
            std::ofstream(file) << "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\n" << extra;
            return file;
        }

        static std::vector<uint8> Encoded(std::u16string const& text)
        {
            SystemMessages::ServerMessage message;
            message.Modal = 1;
            message.Message = text;
            ByteBuffer body;
            sMessageRegistry.Encode(message, body);
            return std::vector<uint8>(body.GetData().begin(), body.GetData().end());
        }

        BaseTable _table;
        LogTestDirectory _directory;
        LogTestHarness _harness;
        ConfigMgr _config;
        std::ostringstream _out;
        std::ostringstream _err;
        std::filesystem::path _install;
    };
}

TEST_F(MessageReloadTest, ABrokenDefinitionKeepsTheServingGenerationAndAMendedOneReplacesIt)
{
    MessageApp app({ "reloadserver", "reloadserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.Install = _install;
    std::filesystem::path const config = WriteConfig();
    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "reloadserver", "-c", ConfigMgr::PathToUtf8(config) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!app.IsReady() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(app.IsReady());
    ASSERT_TRUE(app.Loaded);

    std::vector<std::string> const targets = sReloadMgr.GetOrderedTargets();
    ASSERT_NE(std::find(targets.begin(), targets.end(), "messages"), targets.end()) << "an app that loaded its definitions from an install can reload them";
    uint64 const serving = sMessageRegistry.GetGeneration();
    std::vector<uint8> const before = Encoded(u"Still here");
    ASSERT_FALSE(before.empty());

    WriteRoot(BrokenExtendedBase);
    ReloadOutcome const refused = sReloadMgr.Reload("messages");
    EXPECT_FALSE(refused.Ok);
    ASSERT_FALSE(refused.Errors.empty());
    EXPECT_TRUE(std::any_of(refused.Errors.begin(), refused.Errors.end(), [](std::string const& error) { return error.find("NOTATYPE") != std::string::npos; }))
        << refused.Errors.front();
    EXPECT_EQ(sMessageRegistry.GetGeneration(), serving) << "a refused reload leaves the generation that was serving";
    EXPECT_EQ(Encoded(u"Still here"), before) << "and the messages the app declared still encode as they did";

    WriteRoot(BaseMessageFixtures::ExtendedBaseXml);
    ReloadOutcome const mended = sReloadMgr.Reload("messages");
    ASSERT_TRUE(mended.Ok) << (mended.Errors.empty() ? std::string() : mended.Errors.front());
    EXPECT_GT(sMessageRegistry.GetGeneration(), serving);
    EXPECT_EQ(Encoded(u"Still here"), before);

    app.RequestStop();
    runner.join();
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
}

TEST_F(MessageReloadTest, ABrokenDefinitionReloadedOverTheAdminApiAnswersEveryErrorAndKeepsServing)
{
    MessageApp app({ "reloadserver", "reloadserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.Install = _install;
    std::filesystem::path const config = WriteConfig(std::string("Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 0\nAdmin.Token = ") + AdminToken + "\n");
    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "reloadserver", "-c", ConfigMgr::PathToUtf8(config) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!app.IsReady() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(app.IsReady());
    ASSERT_NE(app.GetAdminApi(), nullptr);
    uint16 const port = app.GetAdminApi()->GetPort();
    uint64 const serving = sMessageRegistry.GetGeneration();
    uint64 const target = sReloadMgr.GetGeneration("messages");
    std::vector<uint8> const before = Encoded(u"Still here");
    ASSERT_FALSE(before.empty());

    WriteRoot(TwoBrokenExtendedBase);
    AdminTest::HttpReply const run = AdminTest::Ask(port, "POST", "/api/reload/messages", AdminToken);
    ASSERT_EQ(run.Status, 409) << run.Body;
    nlohmann::json const answer = nlohmann::json::parse(run.Body, nullptr, false);
    ASSERT_TRUE(answer.is_object()) << run.Body;
    EXPECT_EQ(answer["ok"], false);
    ASSERT_EQ(answer["targets"].size(), 1u);
    EXPECT_TRUE(NamesBoth(answer["targets"][0]["errors"])) << "every error is answered, not the first: " << run.Body;
    EXPECT_EQ(answer["targets"][0]["generation"], target);
    EXPECT_TRUE(answer["targets"][0]["finished_ms"].is_number_integer());

    EXPECT_EQ(sMessageRegistry.GetGeneration(), serving) << "the generation that was serving goes on serving";
    EXPECT_EQ(Encoded(u"Still here"), before);

    nlohmann::json const listing = nlohmann::json::parse(AdminTest::Get(port, "/api/reload", AdminToken).Body, nullptr, false);
    bool listed = false;
    for (nlohmann::json const& entry : listing["targets"])
        if (entry["target"] == "messages")
        {
            listed = true;
            EXPECT_EQ(entry["ok"], false);
            EXPECT_EQ(entry["generation"], target);
            EXPECT_TRUE(NamesBoth(entry["errors"])) << entry.dump();
        }
    EXPECT_TRUE(listed);

    nlohmann::json const events = nlohmann::json::parse(AdminTest::Get(port, "/api/events/after/0", AdminToken).Body, nullptr, false);
    bool announced = false;
    for (nlohmann::json const& event : events["records"])
        if (event["kind"] == "reload.result" && event["subject"] == "messages")
        {
            announced = true;
            EXPECT_EQ(event["data"]["ok"], false);
            EXPECT_TRUE(NamesBoth(event["data"]["errors"])) << event.dump();
        }
    EXPECT_TRUE(announced) << "the event feed reports the failed reload: " << events.dump();

    app.RequestStop();
    runner.join();
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
}
