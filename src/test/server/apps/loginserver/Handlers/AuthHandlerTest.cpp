/*
 * Project Ambrose by Imjustchico
 * Drives MSG_USER_AUTHEN_V3 over loopback against a real LoginSession: a closed login database times out, and with AMBROSE_TEST_DB set valid credentials are admitted with a stored session key hash, a wrong session id, wrong ClientKey1, oversized or malformed Rec1, unknown account, banned machine, banned address, locked or banned account and disallowed revision each get their error and store no session, account bans stay hidden behind a wrong password, the attempt limit is read live and locks the address out, overlapping requests strike and a client that leaves mid-login leaves no claim or reservation behind, duplicate logins kick each earlier session or are rejected, verifiers are sealed again with the active key at login, and the older authentication messages are refused until the session closes.
 */

#include "AccountMgr.h"
#include "Base64.h"
#include "ClientKey.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "FrameWriter.h"
#include "LogTestDirectory.h"
#include "LoginMgr.h"
#include "LoginTestHarness.h"
#include "LoginSession.h"
#include "Rec1.h"
#include "SHA256.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <random>

namespace
{
    using namespace LoginTesting;

    constexpr uint64 Machine = 0x1122334455667788ull;

    std::string Credentials(LoginClient const& client, std::string_view username, std::string_view password, std::optional<uint16> sessionId = std::nullopt)
    {
        std::string const clientKey1 = ClientKey::ComputeClientKey1(ClientKey::HashPassword(password), client.Salt);
        return fmt::format("{} {} {}", sessionId.value_or(client.Salt.SessionId), username, clientKey1);
    }

    LoginMessages::UserAuthenV3 Authen(LoginClient const& client, std::string_view plain, std::string revision = "r0.Test", uint64 machine = Machine)
    {
        LoginMessages::UserAuthenV3 authen;
        authen.Rec1 = Rec1::Encode(plain, client.Salt);
        authen.Version = "W.1.610.0";
        authen.Revision = std::move(revision);
        authen.MachineId = machine;
        authen.Locale = "enUS";
        return authen;
    }

    void SendAuthen(LoginClient& client, std::string_view plain, std::string revision = "r0.Test", uint64 machine = Machine)
    {
        Send(client, Authen(client, plain, std::move(revision), machine));
    }

    std::string HashSessionKey(std::string_view sessionKey)
    {
        return Base64::Encode(SHA256::GetDigestOf(sessionKey));
    }

    class AuthHandlerDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_auth_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "login", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            _open = true;
            sAccountMgr.SetSettings(AccountSettings{});
            sLoginMgr.Reset();
            ASSERT_EQ(sAccountMgr.CreateAccount("Wizard", "hunter22", {}, &_accountId), AccountOpResult::Ok);
            _server = std::make_unique<LoginServerHarness>();
        }

        void TearDown() override
        {
            if (_open)
                LoginDatabase.Close();
            _server.reset();
            sLoginMgr.Reset();
            sAccountMgr.SetSettings(AccountSettings{});
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        uint64 Count(std::string const& sql)
        {
            QueryResult const result = LoginDatabase.Query(sql);
            return result ? (*result)[0].Get<uint64>() : 0;
        }

        void ExpectFailure(LoginClient& client, AuthResult expected, std::string_view scenario)
        {
            std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
            ASSERT_TRUE(response) << scenario;
            EXPECT_EQ(response->Error, expected) << scenario;
            EXPECT_EQ(response->Reason, AuthResults::GetName(expected)) << scenario;
            EXPECT_EQ(response->UserId, 0u) << scenario;
            EXPECT_TRUE(response->Rec1.empty()) << scenario;
            EXPECT_EQ(Count("SELECT COUNT(*) FROM `account_session`"), 0u) << scenario;
        }

        std::string ExpectAdmitted(LoginClient& client)
        {
            std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
            EXPECT_TRUE(response);
            if (!response)
                return {};
            EXPECT_EQ(response->Error, AuthResult::Success);
            EXPECT_EQ(response->UserId, _accountId);
            EXPECT_EQ(response->Reason, "");
            EXPECT_EQ(response->PayingUser, 1);
            std::optional<LoginMessages::UserAdmitInd> const admit = ReadMessage<LoginMessages::UserAdmitInd>(client);
            EXPECT_TRUE(admit);
            if (admit)
            {
                EXPECT_EQ(admit->Status, 1);
                EXPECT_EQ(admit->PositionInQueue, 0u);
            }
            return Rec1::Decode(response->Rec1, client.Salt);
        }

        MySQLConnectionInfo _info;
        bool _open = false;
        uint64 _accountId = 0;
        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST(AuthHandlerTest, AClosedLoginDatabaseTimesOutAndCloses)
{
    sLoginMgr.Reset();
    LoginServerHarness server;
    LoginClient client = server.Connect();
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"));
    std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->Error, AuthResult::Timeout);
    EXPECT_EQ(response->Reason, "Timeout");
    EXPECT_TRUE(client.Socket->WaitForClose());
    sLoginMgr.Reset();
}

TEST_F(AuthHandlerDatabaseTest, ValidCredentialsAreAdmittedWithAStoredSessionKey)
{
    LoginClient client = _server->Connect();
    SendAuthen(client, Credentials(client, "wizard", "hunter22"));
    std::string const sessionKey = ExpectAdmitted(client);
    EXPECT_EQ(sessionKey.size(), 44u);

    QueryResult const row = LoginDatabase.Query(fmt::format("SELECT `session_key_hash`, `machine_id`, `expires` - `created` FROM `account_session` WHERE `account_id` = {}", _accountId));
    ASSERT_TRUE(row);
    EXPECT_EQ((*row)[0].Get<std::string>(), HashSessionKey(sessionKey));
    EXPECT_NE((*row)[0].Get<std::string>(), sessionKey);
    EXPECT_EQ((*row)[1].Get<uint64>(), Machine);
    EXPECT_EQ((*row)[2].Get<uint64>(), 30u * 3600);

    AccountLookup const account = sAccountMgr.GetAccountById(_accountId);
    ASSERT_TRUE(account.Account);
    EXPECT_GT(account.Account->LastLogin, 0u);
    EXPECT_EQ(account.Account->LastIp, "127.0.0.1");
    EXPECT_EQ(account.Account->LastMachineId, Machine);

    std::shared_ptr<LoginSession> const session = sLoginMgr.FindAccountSession(_accountId);
    ASSERT_TRUE(session);
    EXPECT_EQ(session->GetSessionId(), client.Salt.SessionId);
    EXPECT_EQ(session->GetStatus(), SessionStatus::Authenticated);
    EXPECT_EQ(session->GetAccountId(), _accountId);

    SendAuthen(client, Credentials(client, "wizard", "hunter22"));
    EXPECT_FALSE(ReadDml(*client.Socket, std::chrono::milliseconds(300)));
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(AuthHandlerDatabaseTest, EachFailureGetsItsErrorAndStoresNoSession)
{
    LoginSettings settings;
    settings.MaxAuthAttempts = 100;
    settings.EnforceRevision = true;
    settings.AllowedRevisions = { "r0.Test" };
    sLoginMgr.SetSettings(settings);
    LoginClient client = _server->Connect();

    SendAuthen(client, Credentials(client, "Wizard", "hunter22", static_cast<uint16>(client.Salt.SessionId + 1)));
    ExpectFailure(client, AuthResult::AuthenFailed, "wrong session id");
    SendAuthen(client, Credentials(client, "Wizard", "hunter23"));
    ExpectFailure(client, AuthResult::AuthenFailed, "wrong ClientKey1");
    SendAuthen(client, Credentials(client, "Warlock", "hunter22"));
    ExpectFailure(client, AuthResult::AuthenFailed, "unknown account");
    SendAuthen(client, "not three parts");
    ExpectFailure(client, AuthResult::AuthenFailed, "malformed Rec1");
    SendAuthen(client, std::string(LoginSession::MaxRec1Bytes + 1, 'x'));
    ExpectFailure(client, AuthResult::AuthenFailed, "oversized Rec1");
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r1.Other");
    ExpectFailure(client, AuthResult::ErrorNoLock, "disallowed revision");

    uint64 const now = AccountMgr::Now();
    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("INSERT INTO `machine_banned` VALUES ({}, {}, 0, 'test', 'test')", Machine, now)));
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"));
    ExpectFailure(client, AuthResult::MachineBanned, "banned machine");
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r0.Test", Machine + 1);
    std::string const admittedElsewhere = "machine ban only covers its machine";

    std::optional<LoginMessages::UserAuthenRsp> const other = ReadMessage<LoginMessages::UserAuthenRsp>(client);
    ASSERT_TRUE(other) << admittedElsewhere;
    EXPECT_EQ(other->Error, AuthResult::Success) << admittedElsewhere;
    ASSERT_TRUE(ReadMessage<LoginMessages::UserAdmitInd>(client));
    ASSERT_TRUE(LoginDatabase.DirectExecute("DELETE FROM `account_session`"));
    client = _server->Connect();
    ASSERT_TRUE(WaitForCondition([&] { return sLoginMgr.GetAccountSessionCount() == 0; }));

    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("INSERT INTO `ip_banned` VALUES ('127.0.0.1', {}, {}, 'test', 'test')", now, now + 3600)));
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r0.Test", Machine + 1);
    ExpectFailure(client, AuthResult::MachineBanned, "banned address");
    ASSERT_TRUE(LoginDatabase.DirectExecute("UPDATE `ip_banned` SET `unbandate` = 1"));

    ASSERT_EQ(sAccountMgr.SetLocked(_accountId, true), AccountOpResult::Ok);
    SendAuthen(client, Credentials(client, "Wizard", "wrong"), "r0.Test", Machine + 1);
    ExpectFailure(client, AuthResult::AuthenFailed, "locked account with a wrong password");
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r0.Test", Machine + 1);
    ExpectFailure(client, AuthResult::AccountBanned, "locked account");
    ASSERT_EQ(sAccountMgr.SetLocked(_accountId, false), AccountOpResult::Ok);

    ASSERT_EQ(sAccountMgr.Ban(_accountId, std::chrono::hours(1), "test", "testing"), AccountOpResult::Ok);
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r0.Test", Machine + 1);
    ExpectFailure(client, AuthResult::AccountBanned, "banned account");
    ASSERT_EQ(sAccountMgr.Unban(_accountId), AccountOpResult::Ok);

    SendAuthen(client, Credentials(client, "Wizard", "hunter22"), "r0.Test", Machine + 1);
    EXPECT_EQ(ExpectAdmitted(client).size(), 44u);
    EXPECT_EQ(Count("SELECT COUNT(*) FROM `account_session`"), 1u);
}

TEST_F(AuthHandlerDatabaseTest, TheAttemptLimitIsReadLiveAndLocksTheAddressOut)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("loginserver.conf", "Login.MaxAuthAttempts = 3\nLogin.LockoutSeconds = 600\n");
    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    sLoginMgr.LoadSettings(config);

    LoginClient first = _server->Connect();
    SendAuthen(first, Credentials(first, "Wizard", "wrong"));
    ExpectFailure(first, AuthResult::AuthenFailed, "first wrong password");
    EXPECT_FALSE(first.Socket->IsClosed());

    directory.Write("loginserver.conf", "Login.MaxAuthAttempts = 2\nLogin.LockoutSeconds = 600\n");
    ASSERT_TRUE(config.Reload().Succeeded());
    sLoginMgr.LoadSettings(config);
    SendAuthen(first, Credentials(first, "Wizard", "wrong"));
    ExpectFailure(first, AuthResult::AuthenFailed, "second wrong password");
    EXPECT_TRUE(first.Socket->WaitForClose());

    LoginClient second = _server->Connect();
    SendAuthen(second, Credentials(second, "Wizard", "hunter22"));
    ExpectFailure(second, AuthResult::AuthenFailed, "correct password while locked out");
    EXPECT_TRUE(second.Socket->WaitForClose());

    EXPECT_TRUE(sLoginMgr.GetThrottle().Reset(asio::ip::make_address("127.0.0.1")));
    LoginClient third = _server->Connect();
    SendAuthen(third, Credentials(third, "Wizard", "hunter22"));
    EXPECT_EQ(ExpectAdmitted(third).size(), 44u);
}

TEST_F(AuthHandlerDatabaseTest, DuplicateLoginsKickTheEarlierSessionOrAreRejected)
{
    LoginClient first = _server->Connect();
    SendAuthen(first, Credentials(first, "Wizard", "hunter22"));
    ExpectAdmitted(first);

    LoginClient middle = _server->Connect();
    SendAuthen(middle, Credentials(middle, "Wizard", "hunter22"));
    ExpectAdmitted(middle);
    std::optional<SystemMessages::ForceDisconnect> const kicked = ReadMessage<SystemMessages::ForceDisconnect>(first);
    ASSERT_TRUE(kicked);
    EXPECT_EQ(kicked->Type, LoginSession::DisconnectLoggedInElsewhere);
    EXPECT_TRUE(first.Socket->WaitForClose());

    LoginClient second = _server->Connect();
    SendAuthen(second, Credentials(second, "Wizard", "hunter22"));
    std::string const secondKey = ExpectAdmitted(second);
    ASSERT_TRUE(ReadMessage<SystemMessages::ForceDisconnect>(middle));
    EXPECT_TRUE(middle.Socket->WaitForClose());
    std::shared_ptr<LoginSession> const holder = sLoginMgr.FindAccountSession(_accountId);
    ASSERT_TRUE(holder);
    EXPECT_EQ(holder->GetSessionId(), second.Salt.SessionId);
    EXPECT_EQ(sLoginMgr.GetAccountSessionCount(), 1u);
    QueryResult const row = LoginDatabase.Query(fmt::format("SELECT `session_key_hash` FROM `account_session` WHERE `account_id` = {}", _accountId));
    ASSERT_TRUE(row);
    EXPECT_EQ((*row)[0].Get<std::string>(), HashSessionKey(secondKey));

    LoginSettings settings;
    settings.DuplicateLogins = DuplicateLoginPolicy::Reject;
    sLoginMgr.SetSettings(settings);
    LoginClient third = _server->Connect();
    SendAuthen(third, Credentials(third, "Wizard", "hunter22"));
    std::optional<LoginMessages::UserAuthenRsp> const refused = ReadMessage<LoginMessages::UserAuthenRsp>(third);
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->Error, AuthResult::AuthenFailed);
    std::optional<SystemMessages::ServerMessage> const notice = ReadMessage<SystemMessages::ServerMessage>(third);
    ASSERT_TRUE(notice);
    EXPECT_EQ(notice->Message, u"This account is already logged in.");
    EXPECT_EQ(sLoginMgr.FindAccountSession(_accountId), holder);
    EXPECT_FALSE(second.Socket->IsClosed());
}

TEST_F(AuthHandlerDatabaseTest, OverlappingRequestsStrikeAndALeavingClientLeavesNothingBehind)
{
    LoginClient client = _server->Connect();
    ByteBuffer frame;
    std::vector<DmlMessageData> messages(2);
    for (DmlMessageData& message : messages)
    {
        ByteBuffer body;
        sMessageRegistry.Encode(Authen(client, Credentials(client, "Wizard", "hunter22")), body);
        message.ServiceId = LoginMessages::LoginService;
        message.Order = 27;
        message.Body.assign(body.GetData().begin(), body.GetData().end());
    }
    FrameWriter::WriteDml(frame, messages);
    client.Socket->Send(frame);
    EXPECT_EQ(ExpectAdmitted(client).size(), 44u);
    std::shared_ptr<LoginSession> const session = sLoginMgr.FindAccountSession(_accountId);
    ASSERT_TRUE(session);
    EXPECT_EQ(session->GetStrikes(), 1u);
    client.Socket.reset();
    ASSERT_TRUE(WaitForCondition([&] { return sLoginMgr.GetAccountSessionCount() == 0; }));

    for (int i = 0; i < 5; ++i)
    {
        LoginClient leaving = _server->Connect();
        SendAuthen(leaving, Credentials(leaving, "Wizard", "hunter22"));
        leaving.Socket.reset();
    }
    asio::ip::address const loopback = asio::ip::make_address("127.0.0.1");
    EXPECT_TRUE(WaitForCondition([&] { return sLoginMgr.GetThrottle().GetInFlightCount(loopback) == 0; }));
    EXPECT_TRUE(WaitForCondition([&] { return sLoginMgr.GetAccountSessionCount() == 0; }));

    LoginClient last = _server->Connect();
    SendAuthen(last, Credentials(last, "Wizard", "hunter22"));
    EXPECT_EQ(ExpectAdmitted(last).size(), 44u);
    EXPECT_EQ(sLoginMgr.GetAccountSessionCount(), 1u);
}

TEST_F(AuthHandlerDatabaseTest, VerifiersAreSealedAgainWithTheActiveKeyAtLogin)
{
    std::string const key = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    std::string error;
    std::optional<VerifierKeyRing> keys = VerifierKeyRing::Parse("1:" + key, 1, error);
    ASSERT_TRUE(keys) << error;
    AccountSettings settings;
    settings.Keys = std::move(*keys);
    sAccountMgr.SetSettings(std::move(settings));
    ASSERT_EQ(sAccountMgr.GetAccountById(_accountId).Account->VerifierKeyId, 0u);

    LoginClient client = _server->Connect();
    SendAuthen(client, Credentials(client, "Wizard", "hunter22"));
    ExpectAdmitted(client);
    AccountLookup const sealed = sAccountMgr.GetAccountById(_accountId);
    ASSERT_TRUE(sealed.Account);
    EXPECT_EQ(sealed.Account->VerifierKeyId, 1u);
    std::optional<std::string> const verifier = sAccountMgr.GetVerifier(*sealed.Account);
    ASSERT_TRUE(verifier);
    EXPECT_EQ(*verifier, ClientKey::HashPassword("hunter22"));

    LoginClient again = _server->Connect();
    SendAuthen(again, Credentials(again, "Wizard", "hunter22"));
    ExpectAdmitted(again);
    EXPECT_EQ(sAccountMgr.GetAccountById(_accountId).Account->StoredVerifier, sealed.Account->StoredVerifier);

    auto reseal = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VERIFIER_RESEAL);
    ASSERT_TRUE(reseal);
    reseal->SetData(0, std::string("overwritten"));
    reseal->SetData(1, uint8{ 0 });
    reseal->SetData(2, _accountId);
    reseal->SetData(3, std::string("a verifier from before a password change"));
    reseal->SetData(4, uint8{ 1 });
    ASSERT_TRUE(LoginDatabase.DirectExecute(*reseal));
    EXPECT_EQ(sAccountMgr.GetAccountById(_accountId).Account->StoredVerifier, sealed.Account->StoredVerifier);
}

TEST_F(AuthHandlerDatabaseTest, OlderAuthenticationMessagesAreRefused)
{
    LoginSettings settings;
    settings.MaxAuthAttempts = 4;
    sLoginMgr.SetSettings(settings);
    LoginClient client = _server->Connect();
    Send(client, LoginMessages::UserAuthen{ "W.1.610.0" });
    ExpectFailure(client, AuthResult::AuthenFailed, "MSG_USER_AUTHEN");
    Send(client, LoginMessages::UserAuthenV2{ "W.1.610.0" });
    ExpectFailure(client, AuthResult::AuthenFailed, "MSG_USER_AUTHEN_V2");
    Send(client, LoginMessages::WebAuthen{ "W.1.610.0" });
    ExpectFailure(client, AuthResult::AuthenFailed, "MSG_WEB_AUTHEN");
    EXPECT_FALSE(client.Socket->IsClosed());
    Send(client, LoginMessages::WebValidate{ _accountId });
    ExpectFailure(client, AuthResult::AuthenFailed, "MSG_WEB_VALIDATE");
    EXPECT_TRUE(client.Socket->WaitForClose());
}
