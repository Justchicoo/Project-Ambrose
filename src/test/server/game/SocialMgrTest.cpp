/*
 * Project Ambrose by Imjustchico
 * Tests social list invariants, a friend's zone named by its display key or else its path, a friend removal naming no owner taken as the current wizard's, friend acceptance and the live friend cap through real game sessions and an isolated characters database, and verifies ignore filtering through the actual world chat relay.
 */

#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ChatText.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GameTestHarness.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"
#include "SocialMgr.h"
#include "World.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

struct SocialMgrTestAccess
{
    static void PrepareWorld(GameSession& session, uint64 characterId, float x)
    {
        session.SetCharacterId(characterId);
        session.SetStatus(SessionStatus::InWorld);
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session._mapId = 1;
        session._worldGuid = characterId;
        session._publicObject = { 1 };
        session._chatName = "Wizard";
        session._movement.Reset({ x, 0.0f, 0.0f, 0.0f }, 0);
    }
};

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

    std::optional<bool> HasIgnore(uint64 ownerId, uint64 ignoredId)
    {
        auto statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SOCIAL_IGNORES);
        if (!statement)
            return std::nullopt;
        statement->SetData(0, ownerId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return std::nullopt;
        if (!result)
            return false;
        PreparedResultSet const& row = *result;
        do
        {
            if (row[0].Get<uint64>() == ignoredId)
                return true;
        } while (result->NextRow());
        return false;
    }

    template<DeclaredMessage T>
    std::optional<T> ReadReply(FakeSessionClient& client, std::chrono::milliseconds timeout = std::chrono::seconds(20))
    {
        std::optional<DmlMessageData> const reply = ReadNextDml(client, timeout);
        if (!reply || !Is<T>(*reply))
            return std::nullopt;
        T message;
        if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    template<DeclaredMessage T>
    std::optional<T> ReadWorldReply(FakeSessionClient& client, std::string& observed, std::chrono::milliseconds timeout = std::chrono::seconds(30))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            sWorld.Update(std::chrono::milliseconds(2));
            std::optional<DmlMessageData> const data = ReadNextDml(client, std::chrono::milliseconds(10));
            if (!data)
                continue;
            if (!Is<T>(*data))
            {
                MessageInfo const* const info = sMessageRegistry.GetCatalog()->Find(data->ServiceId, data->Order);
                observed += fmt::format("{}:{}, ", data->ServiceId, info ? info->Definition->Tag : std::string("unknown"));
                continue;
            }
            T reply;
            if (sMessageRegistry.GetCatalog()->Decode(data->Body, reply) == MessageDecodeStatus::Ok)
                return reply;
            observed += fmt::format("{}:decode-failed, ", data->ServiceId);
        }
        observed = fmt::format("{} bytes received; {}", client.GetReceivedBytes(), observed);
        return std::nullopt;
    }

    class SocialMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
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
            sWorld.Clear();
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
            {
                EXPECT_TRUE(connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database))));
            }
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
                sWorld.AddSession(session);
                sWorld.Update(std::chrono::milliseconds(50));
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

TEST(SocialMgrTest, AFriendsZoneIsNamedByTheKeyOfItsDisplayNameOrElseByItsPath)
{
    std::map<std::string, ZoneTemplate, std::less<>> zones;
    zones["WizardCity/WC_Hub"] = ZoneTemplate{ .Path = "WizardCity/WC_Hub", .DisplayNameKey = "WizardZone_TheCommons" };
    zones["WizardCity/WC_Unnamed"] = ZoneTemplate{ .Path = "WizardCity/WC_Unnamed" };
    ZoneTemplates const templates(std::move(zones));
    EXPECT_EQ(SocialMgr::FriendZoneName("WizardCity/WC_Hub", &templates), "WizardZone_TheCommons");
    EXPECT_EQ(SocialMgr::FriendZoneName("WizardCity/WC_Unnamed", &templates), "WizardCity/WC_Unnamed");
    EXPECT_EQ(SocialMgr::FriendZoneName("WizardCity/WC_Missing", &templates), "WizardCity/WC_Missing");
    EXPECT_EQ(SocialMgr::FriendZoneName("WizardCity/WC_Hub", nullptr), "WizardCity/WC_Hub");
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

TEST_F(SocialMgrDatabaseTest, AcceptingAnUnsentRequestSendsOnlyTheSelectedChatError)
{
    std::optional<bool> const pending = HasRequest(RequesterId, OwnerId);
    ASSERT_TRUE(pending);
    ASSERT_FALSE(*pending);

    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, OwnerId);
    ASSERT_TRUE(session);
    SocialMgrTestAccess::PrepareWorld(*session, OwnerId, 0.0f);

    GameMessages::BuddyRequestAccept request;
    request.ListOwnerGid = RequesterId;
    request.EntryGid = OwnerId;
    Send(*client, request);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() != 0; })) << "the friend-accept message must reach the game-session handler queue";

    std::string observed;
    std::optional<GameMessages::ChatError> const error = ReadWorldReply<GameMessages::ChatError>(*client, observed);
    ASSERT_TRUE(error) << "no CHATERROR decoded; other DML replies: " << observed;
    EXPECT_EQ(error->ListOwnerGid, OwnerId);
    EXPECT_EQ(error->CharacterId, RequesterId);
    EXPECT_EQ(error->Error, 1u);
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100)));
    std::optional<bool> const stillPending = HasRequest(RequesterId, OwnerId);
    ASSERT_TRUE(stillPending);
    EXPECT_FALSE(*stillPending);
}

TEST_F(SocialMgrDatabaseTest, IgnoringAFriendFiltersTheActualWorldRelay)
{
    ASSERT_TRUE(AddFriendship(OwnerId, RequesterId));
    ASSERT_TRUE(AddFriendship(RequesterId, OwnerId));

    std::unique_ptr<FakeSessionClient> ownerClient;
    std::unique_ptr<FakeSessionClient> speakerClient;
    std::unique_ptr<FakeSessionClient> thirdClient;
    std::shared_ptr<GameSession> const owner = Connect(ownerClient, OwnerId);
    std::shared_ptr<GameSession> const speaker = Connect(speakerClient, RequesterId);
    std::shared_ptr<GameSession> const third = Connect(thirdClient, TargetId);
    ASSERT_TRUE(owner);
    ASSERT_TRUE(speaker);
    ASSERT_TRUE(third);

    SocialMgrTestAccess::PrepareWorld(*owner, OwnerId, 0.0f);
    SocialMgrTestAccess::PrepareWorld(*speaker, RequesterId, 1.0f);
    SocialMgrTestAccess::PrepareWorld(*third, TargetId, 2.0f);

    GameMessages::IgnoreAdd ignore;
    ignore.ListOwnerGid = OwnerId;
    ignore.CharacterGid = RequesterId;
    Send(*ownerClient, ignore);

    ASSERT_TRUE(WaitForCondition([&]
    {
        owner->DrainQueue();
        std::optional<bool> const ignored = HasIgnore(OwnerId, RequesterId);
        return ignored && *ignored;
    })) << "the ignore operation must reach the characters database after its asynchronous list load";
    EXPECT_TRUE(sSocialMgr.IsIgnored(OwnerId, RequesterId));
    EXPECT_FALSE(sSocialMgr.IsFriend(OwnerId, RequesterId));

    for (FakeSessionClient* client : { ownerClient.get(), speakerClient.get() })
        while (client->ReadFrame(std::chrono::milliseconds(100)))
        {
        }

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"ignored speaker");
    Send(*speakerClient, line);
    ASSERT_TRUE(WaitForCondition([&] { return speaker->GetQueuedMessageCount() == 1; }));
    sWorld.Update(std::chrono::milliseconds(50));

    std::optional<DmlMessageData> const ownerLine = ReadNextDml(*ownerClient, std::chrono::milliseconds(200));
    EXPECT_FALSE(ownerLine && Is<GameMessages::RadialChat>(*ownerLine)) << "the world relay must suppress the ignored speaker for its owner";
    std::optional<GameMessages::RadialChat> const thirdLine = ReadReply<GameMessages::RadialChat>(*thirdClient);
    ASSERT_TRUE(thirdLine) << "a third wizard in range still hears the speaker through the world relay";
    EXPECT_EQ(thirdLine->Message, ChatText::Write(u"ignored speaker"));
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
    SocialMgrTestAccess::PrepareWorld(*session, OwnerId, 0.0f);

    GameMessages::BuddyRequestAdd request;
    request.ListOwnerGid = OwnerId;
    request.EntryGid = TargetId;
    Send(*client, request);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() != 0; })) << "the friend-request message must reach the game-session handler queue";

    std::string observed;
    std::optional<GameMessages::ChatError> const error = ReadWorldReply<GameMessages::ChatError>(*client, observed);
    ASSERT_TRUE(error) << "no CHATERROR decoded; other DML replies: " << observed;
    EXPECT_EQ(error->ListOwnerGid, OwnerId);
    EXPECT_EQ(error->CharacterId, TargetId);
    EXPECT_EQ(error->Error, 1u);
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100)));
    std::optional<bool> const pending = HasRequest(OwnerId, TargetId);
    ASSERT_TRUE(pending);
    EXPECT_FALSE(*pending);

    EXPECT_TRUE(sSettings.Set("Social.MaxFriends", std::to_string(previousMaximum), author, "restore the live friend cap").Ok());
}

TEST_F(SocialMgrDatabaseTest, AFriendRemovalThatOmitsTheCurrentWizardAsOwnerDropsTheFriend)
{
    ASSERT_TRUE(AddFriendship(OwnerId, ExistingFriendId));
    ASSERT_TRUE(AddFriendship(ExistingFriendId, OwnerId));

    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, OwnerId);
    ASSERT_TRUE(session);
    SocialMgrTestAccess::PrepareWorld(*session, OwnerId, 0.0f);

    GameMessages::BuddyRequestDrop drop;
    drop.ListOwnerGid = 0;
    drop.EntryGid = ExistingFriendId;
    Send(*client, drop);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() != 0; })) << "the friend removal must reach the game-session handler queue";

    std::string observed;
    std::optional<GameMessages::BuddyDrop> const dropped = ReadWorldReply<GameMessages::BuddyDrop>(*client, observed);
    ASSERT_TRUE(dropped) << "no BUDDYDROP decoded; other DML replies: " << observed;
    EXPECT_EQ(dropped->ListOwnerGid, OwnerId);
    EXPECT_EQ(dropped->EntryGid, ExistingFriendId);
}
