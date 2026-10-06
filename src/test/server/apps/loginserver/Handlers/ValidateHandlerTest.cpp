/*
 * Project Ambrose by Imjustchico
 * Drives MSG_USER_VALIDATE over loopback against a real LoginSession: a closed login database times out and closes, and with AMBROSE_TEST_DB set a PassKey3 made from the session key a login stored and this connection's offer is admitted with MSG_USER_VALIDATE_RSP Error=0 and MSG_USER_ADMIT_IND and renews the key, while one made from the previous connection's offer, from a wrong key, from another machine, for an unknown account, after a password change or a ban, or for a key renewed longer ago than a Login.SessionKeyLifetime lowered without a restart each get only MSG_USER_VALIDATE_RSP with their error and are closed.
 */

#include "AccountMgr.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "LoginMgr.h"
#include "LoginTestHarness.h"
#include "LoginSession.h"
#include "PassKey3.h"
#include "Rec1.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <random>

namespace
{
    using namespace LoginTesting;

    constexpr uint64 Machine = 0x1122334455667788ull;

    LoginMessages::UserValidate Validate(uint64 userId, std::string passKey3, uint64 machine = Machine)
    {
        LoginMessages::UserValidate validate;
        validate.UserId = userId;
        validate.PassKey3 = std::move(passKey3);
        validate.MachineId = machine;
        validate.Locale = "en-US";
        return validate;
    }

    class ValidateHandlerDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_validate_{:08x}", std::random_device()());
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

        std::string LogIn()
        {
            LoginClient client = _server->Connect();
            std::string const clientKey1 = ClientKey::ComputeClientKey1(ClientKey::HashPassword("hunter22"), client.Salt);
            LoginMessages::UserAuthenV3 authen;
            authen.Rec1 = Rec1::Encode(fmt::format("{} Wizard {}", client.Salt.SessionId, clientKey1), client.Salt);
            authen.Version = "W.1.610.0";
            authen.Revision = "r0.Test";
            authen.MachineId = Machine;
            authen.Locale = "enUS";
            Send(client, authen);
            std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
            EXPECT_TRUE(response);
            if (!response || response->Error != AuthResult::Success)
                return {};
            EXPECT_TRUE(ReadMessage<LoginMessages::UserAdmitInd>(client));
            _previous = client.Salt;
            client.Socket.reset();
            return Rec1::Decode(response->Rec1, client.Salt);
        }

        uint64 Renewed()
        {
            QueryResult const row = LoginDatabase.Query(fmt::format("SELECT `renewed` FROM `account_session` WHERE `account_id` = {}", _accountId));
            return row ? (*row)[0].Get<uint64>() : 0;
        }

        void ExpectRefused(LoginClient& client, AuthResult expected, std::string_view scenario)
        {
            std::optional<LoginMessages::UserValidateRsp> const response = ReadMessage<LoginMessages::UserValidateRsp>(client);
            ASSERT_TRUE(response) << scenario;
            EXPECT_EQ(response->Error, expected) << scenario;
            bool const ban = expected == AuthResult::AccountBanned || expected == AuthResult::MachineBanned;
            EXPECT_EQ(response->Reason, ban ? std::string() : std::string(AuthResults::GetName(expected))) << scenario << ": the client shows GUI_<Reason> beside a ban's dated line";
            EXPECT_EQ(response->UserId, 0u) << scenario;
            EXPECT_TRUE(client.Socket->WaitForClose()) << scenario;
            EXPECT_FALSE(sLoginMgr.FindAccountSession(_accountId)) << scenario;
        }

        void ExpectRefusedWith(std::string_view sessionKey, uint64 machine, AuthResult expected, std::string_view scenario)
        {
            LoginClient client = _server->Connect();
            Send(client, Validate(_accountId, PassKey3::Compute(sessionKey, client.Salt), machine));
            ExpectRefused(client, expected, scenario);
        }

        MySQLConnectionInfo _info;
        bool _open = false;
        uint64 _accountId = 0;
        LoginSalt _previous;
        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST(ValidateHandlerTest, AClosedLoginDatabaseTimesOutAndCloses)
{
    sLoginMgr.Reset();
    LoginServerHarness server;
    LoginClient client = server.Connect();
    Send(client, Validate(1, PassKey3::Compute("a session key", client.Salt)));
    std::optional<LoginMessages::UserValidateRsp> const response = ReadMessage<LoginMessages::UserValidateRsp>(client);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->Error, AuthResult::Timeout);
    EXPECT_EQ(response->Reason, "Timeout");
    EXPECT_TRUE(client.Socket->WaitForClose());
    sLoginMgr.Reset();
}

TEST_F(ValidateHandlerDatabaseTest, APassKey3FromTheStoredKeyAndThisOfferIsAdmittedAndRenewsTheKey)
{
    std::string const sessionKey = LogIn();
    ASSERT_EQ(sessionKey.size(), 44u);
    LoginDatabase.DirectExecute(fmt::format("UPDATE `account_session` SET `renewed` = `renewed` - 100 WHERE `account_id` = {}", _accountId));
    uint64 const before = Renewed();

    LoginClient client = _server->Connect();
    Send(client, Validate(_accountId, PassKey3::Compute(sessionKey, client.Salt)));
    std::optional<LoginMessages::UserValidateRsp> const response = ReadMessage<LoginMessages::UserValidateRsp>(client);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->Error, AuthResult::Success);
    EXPECT_EQ(response->Reason, "");
    EXPECT_EQ(response->UserId, _accountId);
    EXPECT_EQ(response->PayingUser, 1);
    EXPECT_EQ(response->Flags, 0);
    std::optional<LoginMessages::UserAdmitInd> const admit = ReadMessage<LoginMessages::UserAdmitInd>(client);
    ASSERT_TRUE(admit);
    EXPECT_EQ(admit->Status, 1);
    EXPECT_EQ(admit->PositionInQueue, 0u);
    std::shared_ptr<LoginSession> const holder = sLoginMgr.FindAccountSession(_accountId);
    ASSERT_TRUE(holder);
    EXPECT_EQ(holder->GetSessionId(), client.Salt.SessionId);
    EXPECT_GE(Renewed(), before + 100);
}

TEST_F(ValidateHandlerDatabaseTest, ThePreviousOfferAWrongKeyAnotherMachineAndAnUnknownAccountAreRefused)
{
    std::string const sessionKey = LogIn();
    ASSERT_FALSE(sessionKey.empty());

    LoginClient replayed = _server->Connect();
    Send(replayed, Validate(_accountId, PassKey3::Compute(sessionKey, _previous)));
    ExpectRefused(replayed, AuthResult::ValidateFailed, "the previous connection's offer");

    ExpectRefusedWith(std::string(44, 'A'), Machine, AuthResult::ValidateFailed, "a wrong key");
    ExpectRefusedWith(sessionKey, Machine + 1, AuthResult::ValidateFailed, "another machine");

    LoginClient stranger = _server->Connect();
    Send(stranger, Validate(_accountId + 1000, PassKey3::Compute(sessionKey, stranger.Salt)));
    ExpectRefused(stranger, AuthResult::ValidateFailed, "an unknown account");

    LoginClient shortKey = _server->Connect();
    Send(shortKey, Validate(_accountId, "short"));
    ExpectRefused(shortKey, AuthResult::ValidateFailed, "a PassKey3 of the wrong length");
}

TEST_F(ValidateHandlerDatabaseTest, LoweringTheLifetimeRefusesAnOlderKeyFromTheNextValidate)
{
    std::string const sessionKey = LogIn();
    ASSERT_FALSE(sessionKey.empty());
    LoginDatabase.DirectExecute(fmt::format("UPDATE `account_session` SET `renewed` = `renewed` - 120 WHERE `account_id` = {}", _accountId));

    LoginSettings settings;
    settings.SessionKeyLifetime = std::chrono::seconds(60);
    sLoginMgr.SetSettings(settings);
    ExpectRefusedWith(sessionKey, Machine, AuthResult::ValidateFailed, "a key renewed before the lowered lifetime");

    settings.SessionKeyLifetime = std::chrono::seconds(3600);
    sLoginMgr.SetSettings(settings);
    LoginClient client = _server->Connect();
    Send(client, Validate(_accountId, PassKey3::Compute(sessionKey, client.Salt)));
    std::optional<LoginMessages::UserValidateRsp> const response = ReadMessage<LoginMessages::UserValidateRsp>(client);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->Error, AuthResult::Success);
}

TEST_F(ValidateHandlerDatabaseTest, APasswordChangeOrABanRevokesTheKey)
{
    std::string sessionKey = LogIn();
    ASSERT_FALSE(sessionKey.empty());
    ASSERT_EQ(sAccountMgr.ChangePassword(_accountId, "hunter23"), AccountOpResult::Ok);
    ExpectRefusedWith(sessionKey, Machine, AuthResult::ValidateFailed, "after a password change");

    ASSERT_EQ(sAccountMgr.ChangePassword(_accountId, "hunter22"), AccountOpResult::Ok);
    sessionKey = LogIn();
    ASSERT_FALSE(sessionKey.empty());
    ASSERT_EQ(sAccountMgr.Ban(_accountId, std::chrono::seconds(0), "tests", "a test ban"), AccountOpResult::Ok);
    ExpectRefusedWith(sessionKey, Machine, AuthResult::ValidateFailed, "after a ban");
}
