/*
 * Project Ambrose by Imjustchico
 * Drives a real LoginSession over loopback with Ambrose-authored definitions: a character list request before authentication is dropped with the session kept, a game message sent to the login server earns a strike, MSG_USER_AUTHEN_V3 reaches its handler decoded with client strings escaped, and refused server messages close the session at the strike limit.
 */

#include "BaseMessageFixtures.h"
#include "FakeSessionClient.h"
#include "FrameWriter.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LoginMessageTable.h"
#include "LoginSession.h"
#include "MessageRegistry.h"
#include "ScopeExit.h"
#include "SocketMgr.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <mutex>
#include <vector>

namespace
{
    constexpr std::string_view LoginFixtureXml = R"(<?xml version="1.0" ?>
<LoginSessionFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">7</ServiceID><ProtocolType TYPE="STR">LOGIN</ProtocolType></RECORD></_ProtocolInfo>
<MSG_REQUESTCHARACTERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">8</_MsgOrder></RECORD></MSG_REQUESTCHARACTERLIST>
<MSG_USER_AUTHEN_RSP><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">14</_MsgOrder><Error TYPE="INT"></Error></RECORD></MSG_USER_AUTHEN_RSP>
<MSG_USER_AUTHEN_V3><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">27</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID><Locale TYPE="STR"></Locale><PatchClientID TYPE="STR"></PatchClientID><IsSteamPatcher TYPE="UINT"></IsSteamPatcher><ConsoleType TYPE="UBYT"></ConsoleType></RECORD></MSG_USER_AUTHEN_V3>
</LoginSessionFixtureMessages>
)";

    constexpr std::string_view GameFixtureXml = R"(<?xml version="1.0" ?>
<LoginSessionGameMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">5</ServiceID><ProtocolType TYPE="STR">GAME</ProtocolType></RECORD></_ProtocolInfo>
<MSG_ATTACH><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">7</_MsgOrder><LoginKey TYPE="STR"></LoginKey></RECORD></MSG_ATTACH>
</LoginSessionGameMessages>
)";

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    ByteBuffer DmlFrame(uint8 serviceId, uint8 order, std::vector<uint8> const& body)
    {
        ByteBuffer frame;
        FrameWriter::WriteDml(frame, serviceId, order, body);
        return frame;
    }
}

TEST(LoginSessionTest, MessagesAreRoutedByStatusWithStrikesForForeignServices)
{
    CapturedLog log;
    sMessageRegistry.Clear();
    ScopeExit const clearRegistry([] { sMessageRegistry.Clear(); });
    std::vector<std::string> errors;
    ASSERT_TRUE(LoginMessageTable::Get().Declare(sMessageRegistry, errors));
    MessageDefinitionSet definitions;
    ASSERT_TRUE(definitions.Add(LoginFixtureXml, "LoginSessionFixtureMessages.xml"));
    ASSERT_TRUE(definitions.Add(GameFixtureXml, "LoginSessionGameMessages.xml"));
    ASSERT_TRUE(BaseMessageFixtures::AddTo(definitions));
    ASSERT_TRUE(sMessageRegistry.Load(std::move(definitions)));

    auto const context = std::make_shared<SessionContext>(SessionSettings{});
    std::mutex sessionsMutex;
    std::vector<std::weak_ptr<LoginSession>> sessions;
    auto manager = std::make_unique<SocketMgr<LoginSession>>([&](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        auto session = std::make_shared<LoginSession>(std::move(socket), limits, context);
        std::lock_guard const lock(sessionsMutex);
        sessions.push_back(session);
        return session;
    });
    ScopeExit const stopNetwork([&manager] { manager.reset(); });
    NetworkSettings network;
    network.BindIp = "127.0.0.1";
    network.Port = 0;
    network.Threads = 1;
    std::string error;
    ASSERT_TRUE(manager->StartNetwork(network, error)) << error;

    FakeSessionClient client(manager->GetPort());
    uint16 const sessionId = client.Handshake();
    ASSERT_NE(sessionId, 0);

    client.Send(DmlFrame(7, 8, {}));
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("received MSG_REQUESTCHARACTERLIST in state Connected, which needs Authenticated"); }));

    client.Send(DmlFrame(5, 7, { 0, 0 }));
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("GAME MSG_ATTACH (5:7), which loginserver never accepts"); }));

    LoginMessages::UserAuthenV3 authen;
    authen.Rec1 = std::string(40, 'x');
    authen.Version = "W.1.610.0";
    authen.Revision = "r0.Test";
    authen.DataRevision = "d1";
    authen.MachineId = 0x0123456789ABCDEFull;
    authen.Locale = "enUS";
    authen.PatchClientId = "patcher";
    authen.IsSteamPatcher = 1;
    authen.ConsoleType = 2;
    ByteBuffer body;
    sMessageRegistry.Encode(authen, body);
    client.Send(DmlFrame(7, 27, std::vector<uint8>(body.GetData().begin(), body.GetData().end())));
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("sent MSG_USER_AUTHEN_V3: version W.1.610.0, revision r0.Test, data revision d1, locale enUS, machine 0123456789ABCDEF, patch client patcher, Steam patcher 1, console type 2, 40-byte Rec1"); }));
    EXPECT_FALSE(log.Contains(authen.Rec1));

    authen.Version = "x\nSession 9 accepted by 10.0.0.5:1 after 3 ms";
    authen.Locale = std::string(500, '\n');
    ByteBuffer forged;
    sMessageRegistry.Encode(authen, forged);
    client.Send(DmlFrame(7, 27, std::vector<uint8>(forged.GetData().begin(), forged.GetData().end())));
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("version x\\x0ASession 9 accepted by 10.0.0.5:1 after 3 ms, revision"); }));
    EXPECT_TRUE(log.Contains("...(500 bytes)"));
    for (LogMessage const& message : log.Store->Messages("Capture"))
        EXPECT_EQ(message.Text.find("x\nSession 9"), std::string::npos);

    std::shared_ptr<LoginSession> session;
    {
        std::lock_guard const lock(sessionsMutex);
        ASSERT_EQ(sessions.size(), 1u);
        session = sessions.front().lock();
    }
    ASSERT_TRUE(session);
    EXPECT_TRUE(session->IsOpen());
    EXPECT_FALSE(session->IsKicked());
    EXPECT_EQ(session->GetStrikes(), 1u);
    EXPECT_EQ(session->GetSessionId(), sessionId);
    client.ReadFrame(std::chrono::milliseconds(200));
    EXPECT_FALSE(client.IsClosed());
}

TEST(LoginSessionTest, RefusedServerMessagesCloseTheSessionAtTheStrikeLimit)
{
    CapturedLog log;
    sMessageRegistry.Clear();
    ScopeExit const clearRegistry([] { sMessageRegistry.Clear(); });
    std::vector<std::string> errors;
    ASSERT_TRUE(LoginMessageTable::Get().Declare(sMessageRegistry, errors));
    MessageDefinitionSet definitions;
    ASSERT_TRUE(definitions.Add(LoginFixtureXml, "LoginSessionFixtureMessages.xml"));
    ASSERT_TRUE(BaseMessageFixtures::AddTo(definitions));
    ASSERT_TRUE(sMessageRegistry.Load(std::move(definitions)));

    SessionSettings settings;
    settings.MaxStrikes = 2;
    auto const context = std::make_shared<SessionContext>(settings);
    auto manager = std::make_unique<SocketMgr<LoginSession>>([&](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        return std::make_shared<LoginSession>(std::move(socket), limits, context);
    });
    ScopeExit const stopNetwork([&manager] { manager.reset(); });
    NetworkSettings network;
    network.BindIp = "127.0.0.1";
    network.Port = 0;
    network.Threads = 1;
    std::string error;
    ASSERT_TRUE(manager->StartNetwork(network, error)) << error;

    FakeSessionClient client(manager->GetPort());
    ASSERT_NE(client.Handshake(), 0);
    ByteBuffer burst;
    for (int i = 0; i < 4; ++i)
        FrameWriter::WriteDml(burst, 7, 14, std::vector<uint8>{ 0, 0, 0, 0 });
    client.Send(burst);
    EXPECT_TRUE(client.WaitForClose());
    EXPECT_TRUE(log.Contains("LOGIN MSG_USER_AUTHEN_RSP (7:14), which only the server sends"));
    EXPECT_TRUE(WaitForCondition([&] { return log.Contains("strike 2 of 2, the last for LOGIN MSG_USER_AUTHEN_RSP (7:14), which only the server sends"); }));
}
