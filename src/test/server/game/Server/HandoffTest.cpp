/*
 * Project Ambrose by Imjustchico
 * Drives the whole handoff over loopback with AMBROSE_TEST_DB set, which is the one thing neither half proves alone: a client signs in to a login server, picks its wizard, is told a gameserver to go to with a key, closes that connection as a real client does, connects to the game server on its own port, handshakes into a session the game server offers of its own, whose id comes from that server's own pool and may repeat the login server's, and sends MSG_ATTACH carrying the key it was given, which the game server dispatches while the session is only Connected. Checks that the key, account and wizard that arrive are the ones the login server issued, that the key is spent so the client is let in, and that the login session is gone rather than waiting on a client that has left, and separately that an attach carrying a key nobody issued is answered with MSG_ATTACHFAILED and the connection closed behind it.
 */

#include "AccountMgr.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "FrameWriter.h"
#include "GameMessageTable.h"
#include "GameSession.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
#include "LoginTestHarness.h"
#include "RealmList.h"
#include "Rec1.h"
#include "SocketMgr.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    using namespace LoginTesting;

    constexpr uint32 HandoffRealmId = 9;
    constexpr uint64 HandoffWizard = 555;

    int64 NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    class GameListener
    {
    public:
        GameListener()
        {
            _context = std::make_shared<SessionContext>(SessionSettings{});
            _manager = std::make_unique<SocketMgr<GameSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                auto session = std::make_shared<GameSession>(std::move(socket), limits, _context);
                std::lock_guard const lock(_mutex);
                _sessions.push_back(session);
                return session;
            });
            NetworkSettings network;
            network.BindIp = "127.0.0.1";
            network.Port = 0;
            network.Threads = 1;
            std::string error;
            EXPECT_TRUE(_manager->StartNetwork(network, error)) << error;
        }

        ~GameListener() { _manager.reset(); }

        uint16 GetPort() const { return _manager->GetPort(); }

        std::shared_ptr<GameSession> Find(uint16 sessionId)
        {
            std::lock_guard const lock(_mutex);
            for (std::weak_ptr<GameSession> const& weak : _sessions)
                if (std::shared_ptr<GameSession> session = weak.lock(); session && session->GetSessionId() == sessionId)
                    return session;
            return nullptr;
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<GameSession>> _manager;
        std::mutex _mutex;
        std::vector<std::weak_ptr<GameSession>> _sessions;
    };

    class HandoffTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            uint32 const suffix = std::random_device()();
            _loginInfo = *info;
            _loginInfo.Database = fmt::format("ambrose_hand_{:08x}", suffix);
            _charactersInfo = *info;
            _charactersInfo.Database = fmt::format("ambrose_hand_{:08x}_characters", suffix);
            ASSERT_TRUE(DBUpdater::Run(_loginInfo, "login", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_charactersInfo, "characters", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_loginInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_charactersInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;

            sAccountMgr.SetSettings(AccountSettings{});
            sLoginMgr.Reset();
            ASSERT_EQ(sAccountMgr.CreateAccount("Wizard", "hunter22", {}, &_accountId), AccountOpResult::Ok);

            CharacterSummary wizard;
            wizard.Guid = HandoffWizard;
            wizard.Account = _accountId;
            wizard.NameIndices = 65793;
            wizard.SchoolId = 2343174;
            wizard.Zone = "WizardCity/WC_Ravenwood";
            wizard.ZoneDisplay = "WizardCity/WC_Ravenwood";
            wizard.Created = 1800000000;
            ASSERT_EQ(CharacterRepository::Create(wizard), CharacterOpResult::Ok);

            _game = std::make_unique<GameListener>();
            GameSession::SetRealmId(HandoffRealmId);

            RealmPolicy policy;
            policy.HeartbeatSeconds = 30;
            policy.OfflineAfterIntervals = 3;
            sRealmList.SetPolicy(policy);
            Realm realm;
            realm.Id = HandoffRealmId;
            realm.Name = "Ambrose";
            realm.Address = "127.0.0.1";
            realm.LocalAddress = "127.0.0.1";
            realm.Port = _game->GetPort();
            realm.LastHeartbeatEpoch = NowSeconds();
            sRealmList.Replace({ realm });

            _login = std::make_unique<LoginServerHarness>();
            MessageDefinitionSet definitions;
            ASSERT_TRUE(LoginMessageFixtures::AddTo(definitions, true));
            ASSERT_TRUE(sMessageRegistry.Load(std::move(definitions)));
            std::vector<std::string> errors;
            ASSERT_TRUE(GameMessageTable::Get().Declare(sMessageRegistry, errors)) << (errors.empty() ? std::string() : errors.front());
        }

        void TearDown() override
        {
            if (_open)
            {
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            _login.reset();
            _game.reset();
            sRealmList.Replace({});
            sLoginMgr.Reset();
            sAccountMgr.SetSettings(AccountSettings{});
            for (MySQLConnectionInfo const* created : { &_loginInfo, &_charactersInfo })
            {
                if (created->Database.empty())
                    continue;
                MySQLConnectionInfo server = *created;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(created->Database)));
            }
        }

        std::unique_ptr<LoginServerHarness> _login;
        std::unique_ptr<GameListener> _game;
        MySQLConnectionInfo _loginInfo;
        MySQLConnectionInfo _charactersInfo;
        uint64 _accountId = 0;
        bool _open = false;
    };
}

TEST_F(HandoffTest, AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo)
{
    LoginClient client = _login->Connect();
    LoginMessages::UserAuthenV3 authen;
    std::string const clientKey1 = ClientKey::ComputeClientKey1(ClientKey::HashPassword("hunter22"), client.Salt);
    authen.Rec1 = Rec1::Encode(fmt::format("{} Wizard {}", client.Salt.SessionId, clientKey1), client.Salt);
    authen.Version = "W.1.610.0";
    authen.Revision = "r0.Test";
    authen.MachineId = 7;
    authen.Locale = "enUS";
    Send(client, authen);
    std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
    ASSERT_TRUE(response && response->Error == AuthResult::Success);
    ASSERT_TRUE(ReadMessage<LoginMessages::UserAdmitInd>(client));

    LoginMessages::SelectCharacter pick;
    pick.CharId = HandoffWizard;
    Send(client, pick);
    std::optional<LoginMessages::CharacterSelected> const selected = ReadMessage<LoginMessages::CharacterSelected>(client);
    ASSERT_TRUE(selected);
    ASSERT_EQ(selected->Error, 0);
    ASSERT_FALSE(selected->Key.empty());
    EXPECT_EQ(selected->TcpPort, static_cast<int32>(_game->GetPort())) << "the client must be sent to the port the game server is actually listening on";

    std::shared_ptr<LoginSession> const loginSession = WaitForSession(*_login, client);
    ASSERT_TRUE(loginSession);
    EXPECT_EQ(loginSession->GetStatus(), SessionStatus::CharacterSelected);

    client.Socket.reset();
    EXPECT_TRUE(WaitForCondition([&] { return !loginSession->IsOpen(); })) << "the login server must let a client that has been sent onward close the connection";

    FakeSessionClient game(static_cast<uint16>(selected->TcpPort));
    uint16 const gameSessionId = game.Handshake();
    ASSERT_NE(gameSessionId, 0);
    std::shared_ptr<GameSession> gameSession;
    ASSERT_TRUE(WaitForCondition([&] { gameSession = _game->Find(gameSessionId); return gameSession != nullptr; }));
    EXPECT_EQ(gameSession->GetStatus(), SessionStatus::Connected);

    GameMessages::Attach attach;
    attach.LoginKey = selected->Key;
    attach.UserId = selected->UserId;
    attach.CharId = selected->CharId;
    attach.ZoneName = selected->ZoneName;
    attach.Location = selected->Location;

    ByteBuffer body;
    sMessageRegistry.Encode(attach, body);
    MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<GameMessages::Attach>();
    ByteBuffer frame;
    FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
    game.Send(frame);

    ASSERT_TRUE(WaitForCondition([&] { return gameSession->GetAccountId() != 0; })) << "the game server did not dispatch MSG_ATTACH while the session was Connected";
    EXPECT_EQ(gameSession->GetAccountId(), _accountId);
    EXPECT_EQ(gameSession->GetCharacterId(), HandoffWizard);
    EXPECT_EQ(gameSession->GetUnhandledMessageCount(), 0u);
}

TEST_F(HandoffTest, AnAttachCarryingAKeyNobodyIssuedIsRefusedAndTheSocketIsClosed)
{
    FakeSessionClient game(_game->GetPort());
    uint16 const gameSessionId = game.Handshake();
    ASSERT_NE(gameSessionId, 0);
    std::shared_ptr<GameSession> gameSession;
    ASSERT_TRUE(WaitForCondition([&] { gameSession = _game->Find(gameSessionId); return gameSession != nullptr; }));

    GameMessages::Attach attach;
    attach.LoginKey = "bm90IGEga2V5IHRoZSBsb2dpbiBzZXJ2ZXIgZXZlciBoYW5kZWQgb3V0";
    attach.UserId = _accountId;
    attach.CharId = HandoffWizard;
    attach.ZoneName = "WizardCity/WC_Ravenwood";
    attach.Location = "-32,-552,-28,6.350083";

    ByteBuffer body;
    sMessageRegistry.Encode(attach, body);
    MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<GameMessages::Attach>();
    ByteBuffer frame;
    FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
    game.Send(frame);

    std::optional<DmlMessageData> const reply = ReadNextDml(game);
    ASSERT_TRUE(reply) << "a client presenting an invented key must be told so rather than ignored";
    MessageInfo const& refusal = sMessageRegistry.GetCatalog()->GetInfo<GameMessages::AttachFailed>();
    EXPECT_EQ(reply->ServiceId, refusal.Protocol->ServiceId);
    EXPECT_EQ(reply->Order, refusal.Definition->Order);

    EXPECT_TRUE(game.WaitForClose()) << "the game server must close a connection it refused";
    EXPECT_FALSE(gameSession->IsAttached());
    EXPECT_EQ(gameSession->GetAccountId(), 0u) << "an invented key must not name an account on the session";
}
