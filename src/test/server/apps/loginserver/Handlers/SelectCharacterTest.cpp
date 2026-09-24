/*
 * Project Ambrose by Imjustchico
 * Drives MSG_SELECTCHARACTER over loopback against a real LoginSession with AMBROSE_TEST_DB set: picking a live wizard of this account while a realm is online answers MSG_CHARACTERSELECTED Error=0 carrying that realm's address and port, the wizard's zone and place, and a key that is in login_key against the right account, wizard and realm; picking another account's wizard, a deleted one, or any wizard while no realm is online answers Error!=0 and leaves login_key empty, so a refused pick hands out nothing anybody could present to a gameserver later.
 */

#include "AccountMgr.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "LocationString.h"
#include "LoginMgr.h"
#include "LoginTestHarness.h"
#include "RealmList.h"
#include "Rec1.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    using namespace LoginTesting;

    constexpr uint32 RealmId = 4;
    constexpr uint16 RealmPort = 12333;

    int64 NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    CharacterSummary MakeWizard(uint64 guid, uint64 account)
    {
        CharacterSummary character;
        character.Guid = guid;
        character.Account = account;
        character.NameIndices = static_cast<uint32>(guid * 65793);
        character.SchoolId = 2343174;
        character.Level = 5;
        character.World = 1;
        character.Zone = "WizardCity/WC_Ravenwood";
        character.ZoneDisplay = "WizardCity/WC_Ravenwood";
        character.PositionX = -32.0f;
        character.PositionY = -552.0f;
        character.PositionZ = -28.0f;
        character.Orientation = 6.350083f;
        character.Created = 1800000000 + guid;
        return character;
    }

    class SelectCharacterTest : public testing::Test
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
            _loginInfo.Database = fmt::format("ambrose_pick_{:08x}", suffix);
            _charactersInfo = *info;
            _charactersInfo.Database = fmt::format("ambrose_pick_{:08x}_characters", suffix);
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
            ASSERT_EQ(sAccountMgr.CreateAccount("Stranger", "hunter33", {}, &_otherAccountId), AccountOpResult::Ok);
            ASSERT_EQ(CharacterRepository::Create(MakeWizard(OwnWizard, _accountId)), CharacterOpResult::Ok);
            ASSERT_EQ(CharacterRepository::Create(MakeWizard(StrangersWizard, _otherAccountId)), CharacterOpResult::Ok);
            ASSERT_EQ(CharacterRepository::Create(MakeWizard(DeletedWizard, _accountId)), CharacterOpResult::Ok);
            ASSERT_EQ(CharacterRepository::SoftDelete(DeletedWizard, _accountId, static_cast<uint64>(NowSeconds())), CharacterOpResult::Ok);

            PutRealmOnline();
            _server = std::make_unique<LoginServerHarness>();
        }

        void TearDown() override
        {
            if (_open)
            {
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            _server.reset();
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

        void PutRealmOnline()
        {
            RealmPolicy policy;
            policy.HeartbeatSeconds = 30;
            policy.OfflineAfterIntervals = 3;
            sRealmList.SetPolicy(policy);
            Realm realm;
            realm.Id = RealmId;
            realm.Name = "Ambrose";
            realm.Address = "203.0.113.7";
            realm.LocalAddress = "127.0.0.1";
            realm.Port = RealmPort;
            realm.LastHeartbeatEpoch = NowSeconds();
            sRealmList.Replace({ realm });
        }

        LoginClient Authenticated()
        {
            LoginClient client = _server->Connect();
            LoginMessages::UserAuthenV3 authen;
            std::string const clientKey1 = ClientKey::ComputeClientKey1(ClientKey::HashPassword("hunter22"), client.Salt);
            authen.Rec1 = Rec1::Encode(fmt::format("{} Wizard {}", client.Salt.SessionId, clientKey1), client.Salt);
            authen.Version = "W.1.610.0";
            authen.Revision = "r0.Test";
            authen.MachineId = 7;
            authen.Locale = "enUS";
            Send(client, authen);
            std::optional<LoginMessages::UserAuthenRsp> const response = ReadMessage<LoginMessages::UserAuthenRsp>(client);
            EXPECT_TRUE(response && response->Error == AuthResult::Success);
            EXPECT_TRUE(ReadMessage<LoginMessages::UserAdmitInd>(client));
            return client;
        }

        std::optional<LoginMessages::CharacterSelected> Pick(uint64 charId, std::string realmName = {})
        {
            LoginClient client = Authenticated();
            LoginMessages::SelectCharacter pick;
            pick.CharId = charId;
            pick.ServerName = std::move(realmName);
            Send(client, pick);
            return ReadMessage<LoginMessages::CharacterSelected>(client);
        }

        uint64 CountKeys()
        {
            QueryResult const result = LoginDatabase.Query("SELECT COUNT(*) FROM `login_key`");
            return result ? (*result)[0].Get<uint64>() : 0;
        }

        static constexpr uint64 OwnWizard = 101;
        static constexpr uint64 StrangersWizard = 202;
        static constexpr uint64 DeletedWizard = 303;

        MySQLConnectionInfo _loginInfo;
        MySQLConnectionInfo _charactersInfo;
        std::unique_ptr<LoginServerHarness> _server;
        uint64 _accountId = 0;
        uint64 _otherAccountId = 0;
        bool _open = false;
    };
}

TEST_F(SelectCharacterTest, AnOwnWizardOnAnOnlineRealmIsSentThereWithAKeyThatWasWrittenDown)
{
    std::optional<LoginMessages::CharacterSelected> const reply = Pick(OwnWizard);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->Error, 0);
    EXPECT_EQ(reply->CharId, OwnWizard);
    EXPECT_EQ(reply->UserId, _accountId);
    EXPECT_EQ(reply->TcpPort, static_cast<int32>(RealmPort));
    EXPECT_EQ(reply->UdpPort, static_cast<int32>(RealmPort));
    EXPECT_EQ(reply->Ip, "127.0.0.1") << "a client on this machine is told the address that reaches it from here";
    EXPECT_EQ(reply->ZoneName, "WizardCity/WC_Ravenwood");
    EXPECT_EQ(reply->Location, "-32,-552,-28,6.350083") << "the place is the compact string the client sends back";
    EXPECT_FALSE(reply->Key.empty());

    QueryResult const stored = LoginDatabase.Query(fmt::format("SELECT `account_id`, `character_guid`, `realm_id`, `used` FROM `login_key` WHERE `key` = '{}'", reply->Key));
    ASSERT_TRUE(stored) << "the key the client was given must be one the gameserver can look up";
    EXPECT_EQ((*stored)[0].Get<uint64>(), _accountId);
    EXPECT_EQ((*stored)[1].Get<uint64>(), OwnWizard);
    EXPECT_EQ((*stored)[2].Get<uint32>(), RealmId);
    EXPECT_EQ((*stored)[3].Get<uint8>(), 0) << "a key that has just been issued has not been spent";
    EXPECT_EQ(CountKeys(), 1u);
}

TEST_F(SelectCharacterTest, AnotherAccountsWizardIsRefusedAndWritesNoKey)
{
    std::optional<LoginMessages::CharacterSelected> const reply = Pick(StrangersWizard);
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->Error, 0);
    EXPECT_TRUE(reply->Key.empty());
    EXPECT_EQ(CountKeys(), 0u) << "a wizard somebody else owns must leave nothing behind to present later";
}

TEST_F(SelectCharacterTest, ADeletedWizardIsRefusedAndWritesNoKey)
{
    std::optional<LoginMessages::CharacterSelected> const reply = Pick(DeletedWizard);
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->Error, 0);
    EXPECT_TRUE(reply->Key.empty());
    EXPECT_EQ(CountKeys(), 0u);
}

TEST_F(SelectCharacterTest, NoRealmOnlineIsRefusedAndWritesNoKey)
{
    sRealmList.Replace({});
    std::optional<LoginMessages::CharacterSelected> const reply = Pick(OwnWizard);
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->Error, 0);
    EXPECT_TRUE(reply->Key.empty());
    EXPECT_EQ(CountKeys(), 0u) << "there is nowhere to send the client, so nothing is issued to get in with";
}

TEST_F(SelectCharacterTest, ARealmThatStoppedBeatingIsRefusedRatherThanUsedAnyway)
{
    RealmPolicy const policy = sRealmList.GetPolicy();
    Realm stale;
    stale.Id = RealmId;
    stale.Name = "Ambrose";
    stale.Address = "203.0.113.7";
    stale.LocalAddress = "127.0.0.1";
    stale.Port = RealmPort;
    stale.LastHeartbeatEpoch = NowSeconds() - static_cast<int64>(policy.HeartbeatSeconds) * (policy.OfflineAfterIntervals + 2);
    sRealmList.Replace({ stale });

    std::optional<LoginMessages::CharacterSelected> const reply = Pick(OwnWizard);
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->Error, 0) << "a realm whose last beat is older than the policy allows is gone, not merely quiet";
    EXPECT_EQ(CountKeys(), 0u);
}

TEST_F(SelectCharacterTest, ANamedRealmThatIsNotThereIsRefusedRatherThanSwappedForAnother)
{
    std::optional<LoginMessages::CharacterSelected> const reply = Pick(OwnWizard, "Somewhere Else");
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->Error, 0) << "a player who asked for one world is not quietly put in a different one";
    EXPECT_EQ(CountKeys(), 0u);
}
