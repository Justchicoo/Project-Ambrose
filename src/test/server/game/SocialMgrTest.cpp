/*
 * Project Ambrose by Imjustchico
 * Tests social list invariants and radial-chat filtering, plus friend acceptance and the live friend cap through real game sessions and an isolated characters database.
 */

#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GameTestHarness.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"
#include "SocialMgr.h"
#include "SpeechRelay.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    using namespace GameTesting;

    constexpr uint64 RequesterId = 7001;
    constexpr uint64 OwnerId = 7002;
    constexpr uint64 TargetId = 7003;
    constexpr uint64 ExistingFriendId = 7004;

    CharacterSummary MakeCharacter(uint64 guid)
    {
        CharacterSummary character;
        character.Guid = guid;
        character.Account = guid + 1000;
        character.NameIndices = static_cast<uint32>(guid);
        character.Created = 1;
        return character;
    }

    std::optional<bool> HasRequest(uint64 requesterId, uint64 targetId)
    {
        auto statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SOCIAL_REQUEST_EXISTS);
        if (!statement)
            return std::nullopt;
        statement->SetData(0, requesterId);
        statement->SetData(1, targetId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return std::nullopt;
        return result && result->GetRowCount() != 0;
    }

    template<DeclaredMessage T>
    std::optional<T> ReadReply(FakeSessionClient& client)
    {
        std::optional<DmlMessageData> const reply = ReadNextDml(client);
        if (!reply || !Is<T>(*reply))
            return std::nullopt;
        T message;
        if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    class SocialMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSocialMgr.Clear();
            sSettings.Clear();

            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_social_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", UpdaterSettings{}));
            std::filesystem::path const sqlPath = DBUpdater::GetBuiltInSourceDirectory() / "data" / "sql" / "updates" / "pending_db_characters" /
                "rev_1790786233_friends.sql";
            std::ifstream sqlFile(sqlPath, std::ios::binary);
            ASSERT_TRUE(sqlFile) << sqlPath.string();
            std::string const sql{ std::istreambuf_iterator<char>(sqlFile), std::istreambuf_iterator<char>() };
            std::string failure;
            ASSERT_TRUE(DBUpdater::ApplyScript(_info, {}, sqlPath.filename().string(), sql, &failure)) << failure;
            CharacterDatabase.Close();
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;

            _configFile = _directory.Write("gameserver.conf", "Player.LinkDeadTime = 1\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));

            for (uint64 const guid : { RequesterId, OwnerId, TargetId, ExistingFriendId })
                ASSERT_EQ(CharacterRepository::Create(MakeCharacter(guid)), CharacterOpResult::Ok) << guid;
        }

        void TearDown() override
        {
            sSocialMgr.Clear();
            sSettings.Clear();
            if (_open)
                CharacterDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                EXPECT_TRUE(connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database))));
        }

        std::shared_ptr<GameSession> Connect(std::unique_ptr<FakeSessionClient>& client, uint64 characterId)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session)
            {
                session->SetCharacterId(characterId);
                session->SetStatus(SessionStatus::LoggedIn);
            }
            return session;
        }

        bool AddFriendship(uint64 ownerId, uint64 friendId)
        {
            auto statement = CharacterDatabase.GetPreparedStatement(CHAR_INS_SOCIAL_FRIEND);
            if (!statement)
                return false;
            statement->SetData(0, ownerId);
            statement->SetData(1, friendId);
            statement->SetData(2, uint8{ 0 });
            statement->SetData(3, uint64{ 1 });
            return CharacterDatabase.DirectExecute(*statement);
        }

        GameDefinitions _definitions;
        GameListener _server;
        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(SocialMgrTest, AcceptingARequestThatWasNeverSentIsRejected)
{
    EXPECT_FALSE(SocialMgr::CanAcceptFriendRequest(false));
    EXPECT_TRUE(SocialMgr::CanAcceptFriendRequest(true));
}

TEST(SocialMgrTest, SocialActionsCanOmitTheCurrentWizardAsOwner)
{
    EXPECT_TRUE(SocialMgr::IsRequestOwnerForCharacter(0, 42));
    EXPECT_TRUE(SocialMgr::IsRequestOwnerForCharacter(42, 42));
    EXPECT_FALSE(SocialMgr::IsRequestOwnerForCharacter(84, 42));
    EXPECT_FALSE(SocialMgr::IsRequestOwnerForCharacter(0, 0));
}

TEST(SocialMgrTest, IncomingRequestsCanOmitTheCurrentWizardAsEntry)
{
    EXPECT_TRUE(SocialMgr::IsIncomingRequestForCharacter(84, 0, 42));
    EXPECT_TRUE(SocialMgr::IsIncomingRequestForCharacter(84, 42, 42));
    EXPECT_FALSE(SocialMgr::IsIncomingRequestForCharacter(0, 0, 42));
    EXPECT_FALSE(SocialMgr::IsIncomingRequestForCharacter(42, 0, 42));
    EXPECT_FALSE(SocialMgr::IsIncomingRequestForCharacter(84, 84, 42));
    EXPECT_FALSE(SocialMgr::IsIncomingRequestForCharacter(84, 0, 0));
}

TEST(SocialMgrTest, IgnoringAFriendRemovesTheFriendshipAndFiltersThePlannedHearers)
{
    SocialLists owner;
    SocialLists other;

    ASSERT_TRUE(owner.AddFriend(RequesterId));
    ASSERT_TRUE(owner.AddIgnore(RequesterId));

    std::vector<SpeechListener> const listeners{
        { .Open = true, .MapId = 1, .X = 0.0f, .Y = 0.0f, .Z = 0.0f },
        { .Open = true, .MapId = 1, .X = 1.0f, .Y = 0.0f, .Z = 0.0f },
        { .Open = true, .MapId = 1, .X = 2.0f, .Y = 0.0f, .Z = 0.0f }
    };
    std::vector<std::size_t> const planned = PlanHearers(listeners, 0, false, 5.0f);
    ASSERT_EQ(planned, (std::vector<std::size_t>{ 1, 2 }));

    std::vector<std::size_t> delivered;
    for (std::size_t const hearer : planned)
    {
        bool const allowed = hearer == 1 ? owner.ShouldRelayChatFrom(RequesterId) : other.ShouldRelayChatFrom(RequesterId);
        if (allowed)
            delivered.push_back(hearer);
    }

    EXPECT_FALSE(owner.IsFriend(RequesterId));
    EXPECT_TRUE(owner.IsIgnored(RequesterId));
    EXPECT_EQ(delivered, (std::vector<std::size_t>{ 2 }));
}

TEST_F(SocialMgrDatabaseTest, AcceptingAnUnsentRequestSendsOnlyTheSelectedChatError)
{
    std::optional<bool> const pending = HasRequest(RequesterId, OwnerId);
    ASSERT_TRUE(pending);
    ASSERT_FALSE(*pending);

    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, OwnerId);
    ASSERT_TRUE(session);

    GameMessages::BuddyRequestAccept request;
    request.ListOwnerGid = RequesterId;
    request.EntryGid = OwnerId;
    Send(*client, request);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);

    std::optional<GameMessages::ChatError> const error = ReadReply<GameMessages::ChatError>(*client);
    ASSERT_TRUE(error);
    EXPECT_EQ(error->ListOwnerGid, OwnerId);
    EXPECT_EQ(error->CharacterId, RequesterId);
    EXPECT_EQ(error->Error, 1u);
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100)));
    std::optional<bool> const stillPending = HasRequest(RequesterId, OwnerId);
    ASSERT_TRUE(stillPending);
    EXPECT_FALSE(*stillPending);
}

TEST_F(SocialMgrDatabaseTest, LoweringTheLiveFriendCapRefusesARequestThroughTheHandler)
{
    ASSERT_TRUE(AddFriendship(OwnerId, ExistingFriendId));
    ASSERT_TRUE(AddFriendship(ExistingFriendId, OwnerId));
    ASSERT_TRUE(AddFriendship(OwnerId, RequesterId));
    ASSERT_TRUE(AddFriendship(RequesterId, OwnerId));

    SettingAuthor const author{ "SocialMgrTest", 0, "unit test" };
    uint32 const previousMaximum = sSettings.Get<uint32>("Social.MaxFriends");
    ASSERT_TRUE(sSettings.Set("Social.MaxFriends", "2", author, "test the live friend cap").Ok());
    ASSERT_EQ(sSettings.Get<uint32>("Social.MaxFriends"), 2u);

    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, OwnerId);
    ASSERT_TRUE(session);

    GameMessages::BuddyRequestAdd request;
    request.ListOwnerGid = OwnerId;
    request.EntryGid = TargetId;
    Send(*client, request);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);

    std::optional<GameMessages::ChatError> const error = ReadReply<GameMessages::ChatError>(*client);
    ASSERT_TRUE(error);
    EXPECT_EQ(error->ListOwnerGid, OwnerId);
    EXPECT_EQ(error->CharacterId, TargetId);
    EXPECT_EQ(error->Error, 1u);
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100)));
    std::optional<bool> const pending = HasRequest(OwnerId, TargetId);
    ASSERT_TRUE(pending);
    EXPECT_FALSE(*pending);

    EXPECT_TRUE(sSettings.Set("Social.MaxFriends", std::to_string(previousMaximum), author, "restore the live friend cap").Ok());
}
