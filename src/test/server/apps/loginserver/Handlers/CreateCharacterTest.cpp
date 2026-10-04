/*
 * Project Ambrose by Imjustchico
 * Drives MSG_CREATECHARACTER over loopback against a real LoginSession with AMBROSE_TEST_DB set, with a world database holding one school, the human name tables and the starting state every school falls back to: a request with every field in order answers MSG_CREATECHARACTERRESPONSE ErrorCode 0 and leaves exactly one wizard stored, standing where the world rows say and owned by the account that asked; a blob that is not a creation info, a school no row knows and a name index past the end of its table each answer ErrorCode 1, write nothing and leave the session able to go on and ask for its character list; an account already holding as many wizards as it is allowed is refused and keeps exactly those it had; and lowering Character.MaxPerAccount refuses the next request with nothing restarted. It drives MSG_DELETECHARACTER over the same session too: the account's own wizard is deleted with ErrorCode 0, its row kept with deleted_at and deleted_account set and gone from the list, while another account's wizard, a missing id and a wizard in the world are refused with ErrorCode 1 and change nothing; Character.DeleteMode hard removes the row, and Character.KeepDeletedDays removes wizards deleted longer ago on the next delete.
 */

#include "AccountMgr.h"
#include "CharacterCreateStore.h"
#include "CharacterDatabase.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "CharacterTypeFixtures.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "LoginMgr.h"
#include "LoginTestHarness.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PendingCharacterUpdates.h"
#include "PropertyObject.h"
#include "Rec1.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    using namespace LoginTesting;

    constexpr uint32 FireSchool = 2343174;
    constexpr uint32 HumanRace = 79806088;
    constexpr uint32 Male = 1;
    constexpr uint32 GoodName = (1u << 16) | (1u << 8) | 1u;

    class CreateCharacterDatabaseTest : public testing::Test
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
            _loginInfo.Database = fmt::format("ambrose_make_{:08x}", suffix);
            _charactersInfo = *info;
            _charactersInfo.Database = fmt::format("ambrose_make_{:08x}_characters", suffix);
            _worldInfo = *info;
            _worldInfo.Database = fmt::format("ambrose_make_{:08x}_world", suffix);
            ASSERT_TRUE(DBUpdater::Run(_loginInfo, "login", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_charactersInfo, "characters", UpdaterSettings{}));
            std::string failure;
            ASSERT_TRUE(AmbroseTestDatabaseUpdates::ApplyPendingFriendsUpdate(_charactersInfo, failure)) << failure;
            ASSERT_TRUE(DBUpdater::Run(_worldInfo, "world", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_loginInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_charactersInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_worldInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;

            FillWorld();
            sAccountMgr.SetSettings(AccountSettings{});
            sLoginMgr.Reset();
            sLoginMgr.ResumeCharacterGuids(0);
            ASSERT_EQ(sAccountMgr.CreateAccount("Wizard", "hunter22", {}, &_accountId), AccountOpResult::Ok);
            sTypeRegistry.SetViews(&_views);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(CharacterTypeFixtures::Dump(), "characters.json"));
            sCharacterNameMgr.SetDefaultLocale("en-US");
            CharacterNameLoadResult const names = sCharacterNameMgr.Load();
            ASSERT_TRUE(names.Loaded) << (names.Errors.empty() ? std::string() : names.Errors.front());
            CharacterCreateLoadResult const rows = sCharacterCreateStore.Load();
            ASSERT_TRUE(rows.Loaded) << (rows.Errors.empty() ? std::string() : rows.Errors.front());
            ASSERT_EQ(rows.Schools, 1u);
            ASSERT_GE(rows.Starts, 1u);
            _server = std::make_unique<LoginServerHarness>();
        }

        void TearDown() override
        {
            if (_open)
            {
                WorldDatabase.Close();
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            _server.reset();
            sCharacterCreateStore.Clear();
            sCharacterNameMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
            sLoginMgr.Reset();
            sAccountMgr.SetSettings(AccountSettings{});
            for (MySQLConnectionInfo const* created : { &_loginInfo, &_charactersInfo, &_worldInfo })
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

        void FillWorld()
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(fmt::format(
                "INSERT INTO `character_create_school` (`school_id`, `school_name`, `sort_order`) VALUES ({}, 'Fire', 0)", FireSchool)));
            ASSERT_TRUE(WorldDatabase.DirectExecute(
                "INSERT INTO `playercreateinfo` (`school_id`, `world`, `zone`, `zone_display`, `position_x`, `position_y`, `position_z`, `orientation`, `level`, `experience`)"
                " VALUES (0, 3, 'WizardCity/WC_Ravenwood', 'Ravenwood', 12.5, -3, 400, 1.5, 1, 0)"
                " ON DUPLICATE KEY UPDATE `world` = VALUES(`world`), `zone` = VALUES(`zone`), `zone_display` = VALUES(`zone_display`),"
                " `position_x` = VALUES(`position_x`), `position_y` = VALUES(`position_y`), `position_z` = VALUES(`position_z`),"
                " `orientation` = VALUES(`orientation`)"));
            for (auto const& [table, index, key, text] : std::vector<std::tuple<char const*, int, char const*, char const*>>{
                     { "FirstName_HumanMale", 0, "First_Boy_0", "Aaron" },
                     { "FirstName_HumanMale", 1, "First_Boy_1", "Blaze" },
                     { "FirstName_HumanFemale", 0, "First_Girl_0", "Abby" },
                     { "MiddleName_Human", 0, "", "" },
                     { "MiddleName_Human", 1, "Middle_0", "Storm" },
                     { "LastName_Human", 0, "", "" },
                     { "LastName_Human", 1, "Last_0", "Blade" } })
                ASSERT_TRUE(WorldDatabase.DirectExecute(fmt::format(
                    "INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES ('{}', 'en-US', {}, '{}', '{}')",
                    table, index, key, text)));
        }

        std::string Blob(uint32 school = FireSchool, uint32 nameIndices = GoodName)
        {
            TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
            PropertyObjectPtr behavior = PropertyObject::Create(catalog, "class WizardCharacterBehavior");
            EXPECT_TRUE(behavior);
            EXPECT_EQ(behavior->Set("m_behaviorTemplateNameID", uint32{ 4242 }), PropertySetResult::Ok);
            EXPECT_EQ(behavior->Set("m_nHairModel", uint32{ 9 }), PropertySetResult::Ok);
            EXPECT_EQ(behavior->Set("m_nHairColor", uint32{ 70 }), PropertySetResult::Ok);
            EXPECT_EQ(behavior->Set("m_eGender", int64{ Male }), PropertySetResult::Ok);
            EXPECT_EQ(behavior->Set("m_eRace", int64{ HumanRace }), PropertySetResult::Ok);

            PropertyObjectPtr info = PropertyObject::Create(catalog, "class WizardCharacterCreationInfo");
            EXPECT_TRUE(info);
            EXPECT_EQ(info->Set("m_templateID", int32{ 1 }), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_schoolOfFocus", school), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_nameIndices", nameIndices), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_avatarBehavior", PropertyValue(std::move(behavior))), PropertySetResult::Ok);

            EncodeResult const encoded = ObjectSerializer::EncodeField(*ObjectFields::Find("MSG_CREATECHARACTER", "CreationInfo"), info.get());
            EXPECT_EQ(encoded.Status, SerializerStatus::Ok) << encoded.Detail;
            return std::string(reinterpret_cast<char const*>(encoded.Bytes.data()), encoded.Bytes.size());
        }

        std::optional<int32> Ask(LoginClient& client, std::string creationInfo)
        {
            LoginMessages::CreateCharacter request;
            request.CreationInfo = std::move(creationInfo);
            Send(client, request);
            std::optional<LoginMessages::CreateCharacterResponse> const response = ReadMessage<LoginMessages::CreateCharacterResponse>(client);
            if (!response)
                return std::nullopt;
            return response->ErrorCode;
        }

        std::optional<int32> Delete(LoginClient& client, uint64 guid)
        {
            LoginMessages::DeleteCharacter request;
            request.CharId = guid;
            Send(client, request);
            std::optional<LoginMessages::DeleteCharacterResponse> const response = ReadMessage<LoginMessages::DeleteCharacterResponse>(client);
            if (!response)
                return std::nullopt;
            return response->ErrorCode;
        }

        std::vector<uint64> Guids()
        {
            CharacterList const stored = CharacterRepository::LoadByAccount(_accountId);
            EXPECT_EQ(stored.Result, CharacterOpResult::Ok);
            std::vector<uint64> guids;
            for (CharacterSummary const& character : stored.Characters)
                guids.push_back(character.Guid);
            std::sort(guids.begin(), guids.end());
            return guids;
        }

        uint32 StoredCharacters() const
        {
            std::optional<uint32> const counted = CharacterRepository::CountByAccount(_accountId);
            EXPECT_TRUE(counted);
            return counted.value_or(0);
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
        MySQLConnectionInfo _worldInfo;
        bool _open = false;
        uint64 _accountId = 0;
        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST_F(CreateCharacterDatabaseTest, AValidRequestStoresExactlyOneWizardWhereTheWorldRowsSay)
{
    LoginClient client = Authenticated();
    ASSERT_EQ(Ask(client, Blob()), 0) << "a request with every field in order is accepted";
    EXPECT_EQ(StoredCharacters(), 1u);

    CharacterList const stored = CharacterRepository::LoadByAccount(_accountId);
    ASSERT_EQ(stored.Result, CharacterOpResult::Ok);
    ASSERT_EQ(stored.Characters.size(), 1u);
    CharacterSummary const& made = stored.Characters.front();
    EXPECT_EQ(made.Account, _accountId);
    EXPECT_NE(made.Guid, 0u);
    EXPECT_EQ(made.SchoolId, FireSchool);
    EXPECT_EQ(made.NameIndices, GoodName);
    EXPECT_EQ(made.Level, 1);
    EXPECT_EQ(made.World, 3);
    EXPECT_EQ(made.Zone, "WizardCity/WC_Ravenwood");
    EXPECT_EQ(made.ZoneDisplay, "Ravenwood");
    EXPECT_FLOAT_EQ(made.PositionX, 12.5f);
    EXPECT_FLOAT_EQ(made.Orientation, 1.5f);
    EXPECT_EQ(made.Appearance.Gender, Male);
    EXPECT_EQ(made.Appearance.Race, HumanRace);
    EXPECT_EQ(made.Appearance.HairModel, 9);
    EXPECT_EQ(made.Appearance.HairColor, 70);
    EXPECT_GT(made.Created, 0u);
    EXPECT_FALSE(made.Online);
    EXPECT_FALSE(made.IsDeleted());
}

TEST_F(CreateCharacterDatabaseTest, ARequestThatIsRefusedWritesNothingAndLeavesTheSessionUsable)
{
    LoginClient client = Authenticated();

    EXPECT_EQ(Ask(client, std::string(64, '\x7F')), 1) << "a blob that is not a creation info is refused";
    EXPECT_EQ(StoredCharacters(), 0u);

    EXPECT_EQ(Ask(client, Blob(1234567)), 1) << "a school no row knows is refused";
    EXPECT_EQ(StoredCharacters(), 0u);

    EXPECT_EQ(Ask(client, Blob(FireSchool, (200u << 16) | (1u << 8) | 1u)), 1) << "a name index past the end of its table is refused";
    EXPECT_EQ(StoredCharacters(), 0u);

    Send(client, LoginMessages::RequestCharacterList{});
    ASSERT_TRUE(ReadMessage<LoginMessages::StartCharacterList>(client)) << "a refusal must not close the session";
    std::optional<LoginMessages::CharacterList> const end = ReadMessage<LoginMessages::CharacterList>(client);
    ASSERT_TRUE(end);
    EXPECT_EQ(end->Error, 0u);

    EXPECT_EQ(Ask(client, Blob()), 0) << "and the session can still create one afterwards";
    EXPECT_EQ(StoredCharacters(), 1u);
}

TEST_F(CreateCharacterDatabaseTest, AnAccountAtItsLimitIsRefusedAndKeepsTheWizardsItHad)
{
    LoginSettings small = *sLoginMgr.GetSettings();
    small.MaxCharactersPerAccount = 2;
    sLoginMgr.SetSettings(small);

    LoginClient client = Authenticated();
    ASSERT_EQ(Ask(client, Blob()), 0);
    ASSERT_EQ(Ask(client, Blob()), 0);
    EXPECT_EQ(StoredCharacters(), 2u);

    EXPECT_EQ(Ask(client, Blob()), 1) << "the third wizard on an account allowed two is refused";
    EXPECT_EQ(StoredCharacters(), 2u) << "and nothing was written for it";

    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("UPDATE `account` SET `purchased_slots` = 1 WHERE `id` = {}", _accountId)));
    EXPECT_EQ(Ask(client, Blob()), 0) << "a purchased slot is a slot";
    EXPECT_EQ(StoredCharacters(), 3u);
}

TEST_F(CreateCharacterDatabaseTest, LoweringTheLimitTakesHoldOnTheNextRequestWithNothingRestarted)
{
    LoginClient client = Authenticated();
    ASSERT_EQ(Ask(client, Blob()), 0);

    LoginSettings small = *sLoginMgr.GetSettings();
    small.MaxCharactersPerAccount = 1;
    sLoginMgr.SetSettings(small);

    EXPECT_EQ(Ask(client, Blob()), 1) << "the limit is read for every request, so the next one is refused without a restart";
    EXPECT_EQ(StoredCharacters(), 1u);
}

TEST_F(CreateCharacterDatabaseTest, EachWizardIsGivenAnIdOfItsOwn)
{
    LoginClient client = Authenticated();
    ASSERT_EQ(Ask(client, Blob()), 0);
    ASSERT_EQ(Ask(client, Blob()), 0);
    ASSERT_EQ(Ask(client, Blob()), 0);

    CharacterList const stored = CharacterRepository::LoadByAccount(_accountId);
    ASSERT_EQ(stored.Result, CharacterOpResult::Ok);
    ASSERT_EQ(stored.Characters.size(), 3u);
    std::vector<uint64> guids;
    for (CharacterSummary const& character : stored.Characters)
        guids.push_back(character.Guid);
    std::sort(guids.begin(), guids.end());
    EXPECT_EQ(std::adjacent_find(guids.begin(), guids.end()), guids.end()) << "no two wizards were given the same id";
    EXPECT_EQ(CharacterRepository::GetMaxGuid().value_or(0), guids.back()) << "and the highest id ever used follows them";
}

TEST_F(CreateCharacterDatabaseTest, AnAccountDeletesItsOwnWizardAndOnlyThatOne)
{
    LoginClient client = Authenticated();
    for (int made = 0; made < 4; ++made)
        ASSERT_EQ(Ask(client, Blob()), 0);
    std::vector<uint64> const guids = Guids();
    ASSERT_EQ(guids.size(), 4u);
    uint64 const own = guids[0];
    uint64 const others = guids[1];
    uint64 const playing = guids[2];
    ASSERT_TRUE(CharacterDatabase.DirectExecute(fmt::format("UPDATE `characters` SET `account` = {} WHERE `guid` = {}", _accountId + 100, others)));
    ASSERT_TRUE(CharacterDatabase.DirectExecute(fmt::format("UPDATE `characters` SET `online` = 1 WHERE `guid` = {}", playing)));

    EXPECT_EQ(Delete(client, own), 0);
    CharacterLoad const deleted = CharacterRepository::Load(own);
    ASSERT_EQ(deleted.Result, CharacterOpResult::Ok) << "a soft delete keeps the row";
    EXPECT_TRUE(deleted.Character->IsDeleted());
    QueryResult const row = CharacterDatabase.Query(fmt::format("SELECT `deleted_at` IS NOT NULL, `deleted_account` FROM `characters` WHERE `guid` = {}", own));
    ASSERT_TRUE(row);
    EXPECT_EQ(row->Fetch()[0].Get<int64>(), 1) << "deleted_at is set";
    EXPECT_EQ(row->Fetch()[1].Get<uint64>(), _accountId) << "and remembers the account a game master can give it back to";
    EXPECT_EQ(Guids(), (std::vector<uint64>{ playing, guids[3] })) << "and the list leaves it out";

    EXPECT_EQ(Delete(client, others), 1) << "another account's wizard";
    EXPECT_EQ(Delete(client, 999999), 1) << "a wizard that does not exist";
    EXPECT_EQ(Delete(client, playing), 1) << "a wizard in the world";
    EXPECT_EQ(Delete(client, own), 1) << "a wizard already deleted";
    EXPECT_EQ(CharacterRepository::Load(others).Character->Account, _accountId + 100) << "and each refusal changed nothing";
    EXPECT_FALSE(CharacterRepository::Load(playing).Character->IsDeleted());
    EXPECT_EQ(Guids().size(), 2u);
}

TEST_F(CreateCharacterDatabaseTest, HardDeletionAndKeptDaysApplyFromTheNextDeleteWithNothingRestarted)
{
    LoginClient client = Authenticated();
    for (int made = 0; made < 3; ++made)
        ASSERT_EQ(Ask(client, Blob()), 0);
    std::vector<uint64> const guids = Guids();
    ASSERT_EQ(guids.size(), 3u);

    ASSERT_EQ(Delete(client, guids[0]), 0);
    ASSERT_TRUE(CharacterDatabase.DirectExecute(fmt::format("UPDATE `characters` SET `deleted_at` = 1 WHERE `guid` = {}", guids[0])));
    LoginSettings changed = *sLoginMgr.GetSettings();
    changed.KeepDeletedDays = 30;
    sLoginMgr.SetSettings(changed);
    ASSERT_EQ(Delete(client, guids[1]), 0);
    EXPECT_TRUE(WaitForCondition([&] { return CharacterRepository::Load(guids[0]).Result == CharacterOpResult::NotFound; }))
        << "a wizard deleted longer ago than Character.KeepDeletedDays is removed on the next delete";
    EXPECT_EQ(CharacterRepository::Load(guids[1]).Result, CharacterOpResult::Ok) << "the one just deleted is kept";

    changed.HardDelete = true;
    sLoginMgr.SetSettings(changed);
    EXPECT_EQ(Delete(client, guids[2]), 0);
    EXPECT_EQ(CharacterRepository::Load(guids[2]).Result, CharacterOpResult::NotFound) << "Character.DeleteMode hard removes the row";
    QueryResult const appearance = CharacterDatabase.Query(fmt::format("SELECT COUNT(*) FROM `character_appearance` WHERE `guid` = {}", guids[2]));
    ASSERT_TRUE(appearance);
    EXPECT_EQ(appearance->Fetch()[0].Get<uint64>(), 0u) << "and what the wizard held with it";
}
