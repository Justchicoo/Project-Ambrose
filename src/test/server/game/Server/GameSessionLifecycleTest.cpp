/*
 * Project Ambrose by Imjustchico
 * Exercises live AFK and link-dead deadlines, reconnect takeover of one character's existing placement, session-owned dirty-stat saves, live potion setting reads and refill timing, and shutdown-safe session state.
 */

#include "ConfigMgr.h"
#include "CharacterRepository.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GameSession.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "PlayerStatsFixtures.h"
#include "Settings.h"
#include "StringHash.h"
#include "World.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct GameSessionLifecycleTestAccess
{
    static void PrepareAttachedInWorld(GameSession& session, uint64 characterId)
    {
        session.SetAccountId(123);
        session.SetCharacterId(characterId);
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session.SetStatus(SessionStatus::InWorld);
    }

    static void Close(GameSession& session)
    {
        session.OnSessionClosed();
    }

    static std::chrono::steady_clock::time_point LostAt(GameSession const& session)
    {
        return std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::nanoseconds(session._socketLostAtNanoseconds.load(std::memory_order_relaxed))));
    }

    static void StartAfkTimer(GameSession& session, std::chrono::steady_clock::time_point started)
    {
        PrepareAttachedInWorld(session, 456);
        session._afkStarted = started;
        session._afkTimerStarted = true;
        session._afkWarned = false;
    }

    static bool WasAfkWarningSent(GameSession const& session)
    {
        return session._afkWarned;
    }

    static std::chrono::steady_clock::time_point AfkStarted(GameSession const& session)
    {
        return session._afkStarted;
    }

    static void SetZonePath(GameSession& session, std::string zonePath)
    {
        session._zonePath = std::move(zonePath);
    }

    static void SetPlayer(GameSession& session, Player player)
    {
        session._player = std::move(player);
    }

    static void TransferWorldState(GameSession& current, GameSession& replacement)
    {
        current.TransferWorldStateTo(replacement);
    }

    static void SetPlacement(GameSession& session, uint32 mapId, uint64 worldGuid, uint16 mobileId)
    {
        session._mapId = mapId;
        session._worldGuid = worldGuid;
        session._zonePath = "WizardCity/Commons";
        session._mobileId = mobileId;
        session._arrived = false;
        session._movement.Reset({ 1.0f, 2.0f, 3.0f, 4.0f }, 0);
        session._movement.Apply(100, 200, 300, 40, 0);
    }
};

namespace
{
    std::optional<PlayerStats> MakeLifecyclePlayerStats(uint64 guid, CharacterStats const& stored)
    {
        CharacterSummary character;
        character.Guid = guid;
        character.Account = guid;
        character.SchoolId = PlayerStatsFixtures::Fire;
        character.Level = 5;

        std::vector<std::string> errors;
        std::shared_ptr<PlayerLevelSet const> const levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
        EXPECT_TRUE(levels) << (errors.empty() ? "" : errors.front());
        if (!levels)
            return std::nullopt;

        std::shared_ptr<StatEffectSet const> const effects = StatEffectSet::Build({ { { "m_shadowPipMax", 2.0 } }, {}, {} }, errors);
        EXPECT_TRUE(effects) << (errors.empty() ? "" : errors.front());
        if (!effects)
            return std::nullopt;

        std::string problem;
        std::optional<PlayerStats> stats = PlayerStats::Create(character, stored, *levels, *effects, problem);
        EXPECT_TRUE(stats) << problem;
        return stats;
    }

    class GameSessionLifecycleTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf",
                "Player.LinkDeadTime = 1\nPlayer.AfkWarnTime = 2\nPlayer.AfkTime = 4\nPotion.RestoreFraction = 0.1\nPotion.RefillInterval = 30\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
            _context = std::make_shared<SessionContext>(SessionSettings{});
        }

        void TearDown() override
        {
            sWorld.Clear();
            sSettings.Clear();
        }

        std::shared_ptr<GameSession> MakeSession()
        {
            asio::ip::tcp::acceptor acceptor(_io, { asio::ip::address_v4::loopback(), 0 });
            asio::ip::tcp::socket client(_io);
            client.connect(acceptor.local_endpoint());
            asio::ip::tcp::socket server(_io);
            acceptor.accept(server);
            return std::make_shared<GameSession>(std::move(server), FrameLimits{}, _context);
        }

        asio::io_context _io;
        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        std::shared_ptr<SessionContext> _context;
    };

    class GameSessionStatsSaveTest : public GameSessionLifecycleTest
    {
    protected:
        void SetUp() override
        {
            GameSessionLifecycleTest::SetUp();
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_session_stats_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", UpdaterSettings{}));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _databaseOpen = true;
        }

        void TearDown() override
        {
            if (_databaseOpen)
                CharacterDatabase.Close();
            if (!_info.Database.empty())
            {
                MySQLConnectionInfo server = _info;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
            }
            GameSessionLifecycleTest::TearDown();
        }

        MySQLConnectionInfo _info;
        bool _databaseOpen = false;
    };
}

TEST_F(GameSessionLifecycleTest, LinkDeadDeadlineUsesTheLiveSettingOnTheNextWorldTimer)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, 42);
    GameSessionLifecycleTestAccess::Close(*session);
    ASSERT_TRUE(session->IsLinkDead());

    auto const lostAt = GameSessionLifecycleTestAccess::LostAt(*session);
    ASSERT_TRUE(sSettings.Set("Player.LinkDeadTime", "3", { "test", 1, "unit_test" }, "extend test window").Ok());
    session->WorldUpdate(lostAt + std::chrono::seconds(2));
    EXPECT_TRUE(session->CanResume(lostAt + std::chrono::seconds(2)));

    ASSERT_TRUE(sSettings.Set("Player.LinkDeadTime", "2", { "test", 1, "unit_test" }, "shorten test window").Ok());
    session->WorldUpdate(lostAt + std::chrono::seconds(2));
    EXPECT_FALSE(session->IsLinkDead());
    EXPECT_FALSE(session->IsAttached());
}

TEST_F(GameSessionLifecycleTest, NotAfkResetsTheWarningAndLiveAfkTimeControlsDisconnect)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    auto const started = std::chrono::steady_clock::now();
    GameSessionLifecycleTestAccess::StartAfkTimer(*session, started);

    session->WorldUpdate(started + std::chrono::seconds(2));
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    EXPECT_FALSE(session->IsKicked());

    GameMessages::NotAfk notAfk;
    session->HandleNotAfk(notAfk);
    auto const resumedAt = GameSessionLifecycleTestAccess::AfkStarted(*session);
    session->WorldUpdate(resumedAt + std::chrono::seconds(1));
    EXPECT_FALSE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    session->WorldUpdate(resumedAt + std::chrono::seconds(2));
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));

    ASSERT_TRUE(sSettings.Set("Player.AfkTime", "3", { "test", 1, "unit_test" }, "shorten test timeout").Ok());
    session->WorldUpdate(resumedAt + std::chrono::seconds(3));
    EXPECT_TRUE(session->IsKicked());
}

TEST_F(GameSessionLifecycleTest, RepeatedClientZonedDoesNotResetAfkTimer)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, 42);
    GameSessionLifecycleTestAccess::SetZonePath(*session, "WizardCity/Commons");
    session->SetStatus(SessionStatus::LoggedIn);

    GameMessages::ClientZoned zoned;
    zoned.ZoneNameId = StringHash::KiStringHash("WizardCity/Commons");
    session->HandleClientZoned(zoned);
    auto const started = GameSessionLifecycleTestAccess::AfkStarted(*session);

    session->WorldUpdate(started + std::chrono::seconds(2));
    ASSERT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    session->HandleClientZoned(zoned);

    EXPECT_EQ(GameSessionLifecycleTestAccess::AfkStarted(*session), started);
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
}

TEST_F(GameSessionLifecycleTest, ReplacementAttachTakesOverTheExistingCharacterPlacement)
{
    std::shared_ptr<GameSession> const current = MakeSession();
    std::shared_ptr<GameSession> const replacement = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*current, 42);
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*replacement, 42);
    GameSessionLifecycleTestAccess::SetPlacement(*current, 17, 42, 9);
    sWorld.AddSession(current);
    sWorld.AddSession(replacement);

    ASSERT_EQ(sWorld.FindSessionByCharacterId(42), current);
    PlayerPosition const position = current->GetMovement().GetPosition();
    GameSessionLifecycleTestAccess::TransferWorldState(*current, *replacement);

    EXPECT_TRUE(current->IsKicked());
    EXPECT_FALSE(current->IsAttached());
    EXPECT_FALSE(current->GetMapId());
    EXPECT_EQ(replacement->GetMapId(), 17u);
    EXPECT_EQ(replacement->GetWorldGuid(), 42u);
    EXPECT_EQ(replacement->GetMovement().GetPosition(), position);
    EXPECT_EQ(sWorld.FindSessionByCharacterId(42), replacement);
}

TEST_F(GameSessionLifecycleTest, AZeroChargePotionDoesNothingAndTheNextUseReadsTheLiveRestoreFraction)
{
    constexpr uint64 Guid = 1800080101;
    CharacterStats stored;
    stored.Health = 20;
    stored.Mana = 5;
    stored.PotionCharge = 0.0f;
    stored.PotionMax = 2.0f;
    std::optional<PlayerStats> stats = MakeLifecyclePlayerStats(Guid, stored);
    ASSERT_TRUE(stats);
    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, Guid);
    GameSessionLifecycleTestAccess::SetPlayer(*session, Player(std::move(*stats)));

    int32 const health = session->GetStats()->GetHitpoints();
    int32 const mana = session->GetStats()->GetMana();
    GameMessages::UsePotion noCharges;
    session->HandleUsePotion(noCharges);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), health);
    EXPECT_EQ(session->GetStats()->GetMana(), mana);
    EXPECT_FLOAT_EQ(session->GetStats()->GetPotionCharge(), 0.0f);

    ASSERT_TRUE(session->SetPotionCapacity(2));
    ASSERT_TRUE(session->SetHealth(0));
    ASSERT_TRUE(session->SetMana(0));
    ASSERT_TRUE(sSettings.Set("Potion.RestoreFraction", "0.25", { "test", 1, "unit_test" }, "use the first live potion fraction").Ok());
    GameMessages::UsePotion firstUse;
    session->HandleUsePotion(firstUse);
    int32 const firstHealth = std::lround(static_cast<double>(session->GetStats()->GetMaxHitpoints()) * 0.25);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), firstHealth);
    EXPECT_FLOAT_EQ(session->GetStats()->GetPotionCharge(), 1.0f);

    ASSERT_TRUE(session->SetHealth(0));
    ASSERT_TRUE(session->SetMana(0));
    ASSERT_TRUE(sSettings.Set("Potion.RestoreFraction", "0.5", { "test", 1, "unit_test" }, "use the next live potion fraction").Ok());
    GameMessages::UsePotion secondUse;
    session->HandleUsePotion(secondUse);
    int32 const secondHealth = std::lround(static_cast<double>(session->GetStats()->GetMaxHitpoints()) * 0.5);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), secondHealth);
    EXPECT_FLOAT_EQ(session->GetStats()->GetPotionCharge(), 0.0f);
}

TEST_F(GameSessionStatsSaveTest, LiveGoldAndPotionChangesPersistBeforeLeavingTheWorld)
{
    constexpr uint64 Guid = 1800080102;
    CharacterSummary character;
    character.Guid = Guid;
    character.Account = Guid;
    character.SchoolId = PlayerStatsFixtures::Fire;
    character.Level = 5;
    character.Zone = "WizardCity/WC_Ravenwood";
    character.ZoneDisplay = "Ravenwood";
    character.Created = 1;
    ASSERT_EQ(CharacterRepository::Create(character), CharacterOpResult::Ok);

    CharacterStats stored;
    stored.Gold = 100;
    stored.Health = 100;
    stored.Mana = 5;
    stored.PotionCharge = 2.0f;
    stored.PotionMax = 2.0f;
    ASSERT_EQ(CharacterRepository::SaveStats(Guid, stored), CharacterOpResult::Ok);
    std::optional<PlayerStats> stats = MakeLifecyclePlayerStats(Guid, stored);
    ASSERT_TRUE(stats);

    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, Guid);
    GameSessionLifecycleTestAccess::SetPlacement(*session, 1, Guid, 1);
    GameSessionLifecycleTestAccess::SetPlayer(*session, Player(std::move(*stats)));
    ASSERT_TRUE(sSettings.Set("Potion.RestoreFraction", "0.25", { "test", 1, "unit_test" }, "restore a quarter of each vital").Ok());

    ASSERT_TRUE(session->SetGold(500));
    GameMessages::UsePotion usePotion;
    session->HandleUsePotion(usePotion);

    CharacterStatsLoad persisted;
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        persisted = CharacterRepository::LoadStats(Guid);
        if (persisted.Stats && persisted.Stats->Gold == 500 && persisted.Stats->PotionCharge == 1.0f && persisted.Stats->Revision >= 2)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_EQ(persisted.Result, CharacterOpResult::Ok);
    ASSERT_TRUE(persisted.Stats);
    EXPECT_EQ(persisted.Stats->Gold, 500);
    EXPECT_FLOAT_EQ(persisted.Stats->PotionCharge, 1.0f);
    EXPECT_EQ(persisted.Stats->Revision, 2u);
}
