/*
 * Project Ambrose by Imjustchico
 * Drives MSG_REQUESTCHARACTERLIST over loopback against a real LoginSession with AMBROSE_TEST_DB set: an authenticated account with three characters gets MSG_STARTCHARACTERLIST with the live server name and its purchased slots, each character's blob in creation order decoding to that character, and MSG_CHARACTERLIST Error=0 while another account's characters stay hidden; an account with none gets the start and end with nothing between; a request made during a listing gets a second full list; a list stops at 256 wizards; a client that leaves mid-listing leaves the server serving others; and a missing account, a closed characters database or a missing type dump answers MSG_CHARACTERLIST Error=1 alone.
 */

#include "AccountMgr.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "CharacterTypeFixtures.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "LoginMgr.h"
#include "LoginScreenInfoBuilder.h"
#include "LoginTestHarness.h"
#include "ObjectFields.h"
#include "Rec1.h"
#include "TypedView.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <random>
#include <vector>

namespace
{
    using namespace LoginTesting;

    CharacterSummary MakeCharacter(uint64 guid, uint64 account)
    {
        CharacterSummary character;
        character.Guid = guid;
        character.Account = account;
        character.NameIndices = static_cast<uint32>(guid * 65793);
        character.SchoolId = 2343174;
        character.Level = static_cast<int32>(guid * 10);
        character.World = 1;
        character.ZoneDisplay = "WizardCity/WC_Ravenwood";
        character.Created = 1800000000 + guid;
        character.Appearance.Gender = static_cast<uint32>(guid % 2);
        character.Appearance.HairColor = static_cast<uint8>(guid * 7 % 64);
        return character;
    }

    class CharacterHandlerDatabaseTest : public testing::Test
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
            _loginInfo.Database = fmt::format("ambrose_list_{:08x}", suffix);
            _charactersInfo = *info;
            _charactersInfo.Database = fmt::format("ambrose_list_{:08x}_characters", suffix);
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
            sTypeRegistry.SetViews(&_views);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(CharacterTypeFixtures::Dump(), "characters.json"));
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
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
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

        void ExpectList(LoginClient& client, std::vector<uint64> const& guids)
        {
            ASSERT_TRUE(ReadMessage<LoginMessages::StartCharacterList>(client));
            for (uint64 const guid : guids)
            {
                std::optional<LoginMessages::CharacterInfo> const info = ReadMessage<LoginMessages::CharacterInfo>(client);
                ASSERT_TRUE(info) << guid;
                DecodeResult const decoded = ObjectSerializer::DecodeField(sTypeRegistry.GetCatalog(), *ObjectFields::Find("MSG_CHARACTERINFO", "CharacterInfo"), std::vector<uint8>(info->Info.begin(), info->Info.end()));
                ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
                EXPECT_EQ(*decoded.Object->Get("m_globalID")->GetIf<uint64>(), guid);
            }
            std::optional<LoginMessages::CharacterList> const end = ReadMessage<LoginMessages::CharacterList>(client);
            ASSERT_TRUE(end);
            EXPECT_EQ(end->Error, 0u);
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

        TypedViewRegistry _views;
        MySQLConnectionInfo _loginInfo;
        MySQLConnectionInfo _charactersInfo;
        bool _open = false;
        uint64 _accountId = 0;
        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST_F(CharacterHandlerDatabaseTest, AnAccountsCharactersAreListedInCreationOrderBetweenStartAndEnd)
{
    CharacterSummary newest = MakeCharacter(1, _accountId);
    newest.Created = 1800000100;
    ASSERT_EQ(CharacterRepository::Create(newest), CharacterOpResult::Ok);
    for (uint64 guid = 2; guid <= 3; ++guid)
        ASSERT_EQ(CharacterRepository::Create(MakeCharacter(guid, _accountId)), CharacterOpResult::Ok);
    ASSERT_EQ(CharacterRepository::Create(MakeCharacter(9, _accountId + 1)), CharacterOpResult::Ok);
    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("UPDATE `account` SET `purchased_slots` = 4 WHERE `id` = {}", _accountId)));
    LoginSettings named = *sLoginMgr.GetSettings();
    named.Name = "Spiral Realm";
    sLoginMgr.SetSettings(named);

    LoginClient client = Authenticated();
    Send(client, LoginMessages::RequestCharacterList{});
    std::optional<LoginMessages::StartCharacterList> const start = ReadMessage<LoginMessages::StartCharacterList>(client);
    ASSERT_TRUE(start);
    EXPECT_EQ(start->LoginServer, "Spiral Realm");
    EXPECT_EQ(start->PurchasedCharacterSlots, 4);

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    ObjectField const& field = *ObjectFields::Find("MSG_CHARACTERINFO", "CharacterInfo");
    for (uint64 const guid : { uint64{ 2 }, uint64{ 3 }, uint64{ 1 } })
    {
        std::optional<LoginMessages::CharacterInfo> const info = ReadMessage<LoginMessages::CharacterInfo>(client);
        ASSERT_TRUE(info) << guid;
        std::vector<uint8> const blob(info->Info.begin(), info->Info.end());
        EXPECT_EQ(blob, LoginScreenInfoBuilder::Encode(catalog, guid == 1 ? newest : MakeCharacter(guid, _accountId)).Bytes) << guid;
        DecodeResult const decoded = ObjectSerializer::DecodeField(catalog, field, blob);
        ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
        EXPECT_EQ(*decoded.Object->Get("m_globalID")->GetIf<uint64>(), guid);
        EXPECT_EQ(*decoded.Object->Get("m_userID")->GetIf<uint64>(), _accountId);
        EXPECT_EQ(*decoded.Object->Get("m_level")->GetIf<int32>(), static_cast<int32>(guid * 10));
    }
    std::optional<LoginMessages::CharacterList> const end = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(end);
    EXPECT_EQ(end->Error, 0u);

    Send(client, LoginMessages::RequestCharacterList{});
    ASSERT_TRUE(ReadMessage<LoginMessages::StartCharacterList>(client));
}

TEST_F(CharacterHandlerDatabaseTest, AnAccountWithoutCharactersGetsAnEmptyList)
{
    LoginClient client = Authenticated();
    Send(client, LoginMessages::RequestCharacterList{});
    std::optional<LoginMessages::StartCharacterList> const start = ReadMessage<LoginMessages::StartCharacterList>(client);
    ASSERT_TRUE(start);
    EXPECT_EQ(start->LoginServer, "Ambrose");
    EXPECT_EQ(start->PurchasedCharacterSlots, 0);
    std::optional<LoginMessages::CharacterList> const end = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(end);
    EXPECT_EQ(end->Error, 0u);
}

TEST_F(CharacterHandlerDatabaseTest, ARequestDuringAListingGetsASecondFullList)
{
    for (uint64 guid = 1; guid <= 2; ++guid)
        ASSERT_EQ(CharacterRepository::Create(MakeCharacter(guid, _accountId)), CharacterOpResult::Ok);
    LoginClient client = Authenticated();
    Send(client, LoginMessages::RequestCharacterList{});
    Send(client, LoginMessages::RequestCharacterList{});
    ExpectList(client, { 1, 2 });
    ExpectList(client, { 1, 2 });

    Send(client, LoginMessages::RequestCharacterList{});
    ExpectList(client, { 1, 2 });
}

TEST_F(CharacterHandlerDatabaseTest, AListStopsAtTheMostWizardsOneAccountShows)
{
    std::vector<uint64> expected;
    for (uint64 guid = 1; guid <= MaxCharactersListed + 1; ++guid)
    {
        ASSERT_EQ(CharacterRepository::Create(MakeCharacter(guid, _accountId)), CharacterOpResult::Ok);
        if (guid <= MaxCharactersListed)
            expected.push_back(guid);
    }
    LoginClient client = Authenticated();
    Send(client, LoginMessages::RequestCharacterList{});
    ExpectList(client, expected);
}

TEST_F(CharacterHandlerDatabaseTest, AClientThatLeavesMidListingLeavesTheServerServingOthers)
{
    ASSERT_EQ(CharacterRepository::Create(MakeCharacter(1, _accountId)), CharacterOpResult::Ok);
    {
        LoginClient leaving = Authenticated();
        Send(leaving, LoginMessages::RequestCharacterList{});
        Send(leaving, LoginMessages::RequestCharacterList{});
    }
    LoginClient client = Authenticated();
    Send(client, LoginMessages::RequestCharacterList{});
    ExpectList(client, { 1 });
}

TEST_F(CharacterHandlerDatabaseTest, AMissingAccountAnswersWithAnError)
{
    ASSERT_EQ(CharacterRepository::Create(MakeCharacter(1, _accountId)), CharacterOpResult::Ok);
    LoginClient client = Authenticated();
    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("DELETE FROM `account` WHERE `id` = {}", _accountId)));
    Send(client, LoginMessages::RequestCharacterList{});
    std::optional<LoginMessages::CharacterList> const end = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(end);
    EXPECT_EQ(end->Error, 1u);
}

TEST_F(CharacterHandlerDatabaseTest, AFailedQueryOrAMissingTypeDumpAnswersWithAnError)
{
    ASSERT_EQ(CharacterRepository::Create(MakeCharacter(1, _accountId)), CharacterOpResult::Ok);
    LoginClient client = Authenticated();

    sTypeRegistry.Clear();
    Send(client, LoginMessages::RequestCharacterList{});
    std::optional<LoginMessages::CharacterList> const unencodable = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(unencodable);
    EXPECT_EQ(unencodable->Error, 1u);

    ASSERT_TRUE(sTypeRegistry.LoadFromText(CharacterTypeFixtures::Dump(), "characters.json"));
    CharacterDatabase.Close();
    Send(client, LoginMessages::RequestCharacterList{});
    std::optional<LoginMessages::CharacterList> const unreadable = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(unreadable);
    EXPECT_EQ(unreadable->Error, 1u);
}
