/*
 * Project Ambrose by Imjustchico
 * Checks the reload an operator runs after the client's message definitions change, through the target a running app registers once it names the install it reads them from: the install is a Root.wad the test builds, a definition broken on disk refuses the reload with its error and leaves the generation that was serving, whose declared messages still encode, and once the file is mended the same reload takes a new generation.
 */

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

        std::filesystem::path WriteConfig()
        {
            std::filesystem::path const file = _directory.Path() / "reloadserver.conf";
            std::ofstream(file) << "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\n";
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
