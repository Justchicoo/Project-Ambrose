/*
 * Project Ambrose by Imjustchico
 * Tests a wizard's vitals as its session changes them: MSG_USEPOTION with less than one charge changes nothing, and a potion restores the share of health and mana Potion.RestoreFraction names at the moment it is drunk, so a changed setting applies from the next potion without a restart; health, mana and gold changed through the session stay within their maximums, and a potion refills one charge each Potion.RefillInterval up to its maximum.
 */

#include "ConfigMgr.h"
#include "GameSession.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "PlayerStatsFixtures.h"
#include "Settings.h"
#include "World.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GameSessionVitalsTestAccess
{
    static void Enter(GameSession& session, PlayerStats stats)
    {
        session.SetAccountId(123);
        session.SetCharacterId(77);
        session._worldGuid = 77;
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session.SetStatus(SessionStatus::InWorld);
        session._stats = std::move(stats);
    }

    static void RefillPotion(GameSession& session, std::chrono::steady_clock::time_point now)
    {
        session.RefillPotion(now);
    }
};

namespace
{
    class GameSessionVitalsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf", "Potion.RestoreFraction = 0.5\nPotion.RefillInterval = 60\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
            _context = std::make_shared<SessionContext>(SessionSettings{});

            _levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
            ASSERT_TRUE(_levels) << errors.front();
            _effects = StatEffectSet::Build({ {}, {}, {} }, errors);
            ASSERT_TRUE(_effects) << errors.front();
        }

        void TearDown() override
        {
            sWorld.Clear();
            sSettings.Clear();
        }

        std::shared_ptr<GameSession> Enter(float potionCharge)
        {
            asio::ip::tcp::acceptor acceptor(_io, { asio::ip::address_v4::loopback(), 0 });
            _client = std::make_unique<asio::ip::tcp::socket>(_io);
            _client->connect(acceptor.local_endpoint());
            asio::ip::tcp::socket server(_io);
            acceptor.accept(server);
            auto session = std::make_shared<GameSession>(std::move(server), FrameLimits{}, _context);
            CharacterSummary character;
            character.Guid = 77;
            character.Account = 123;
            character.SchoolId = PlayerStatsFixtures::Fire;
            character.Level = 1;
            CharacterStats stored;
            stored.Health = 1;
            stored.Mana = 0;
            stored.PotionMax = 3.0f;
            stored.PotionCharge = potionCharge;
            std::string problem;
            std::optional<PlayerStats> stats = PlayerStats::Create(character, stored, *_levels, *_effects, problem);
            EXPECT_TRUE(stats) << problem;
            GameSessionVitalsTestAccess::Enter(*session, std::move(*stats));
            return session;
        }

        asio::io_context _io;
        std::unique_ptr<asio::ip::tcp::socket> _client;
        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        std::shared_ptr<SessionContext> _context;
        std::shared_ptr<PlayerLevelSet const> _levels;
        std::shared_ptr<StatEffectSet const> _effects;
    };
}

TEST_F(GameSessionVitalsTest, UsePotionWithNoWholeChargeChangesNothing)
{
    std::shared_ptr<GameSession> const session = Enter(0.0f);
    GameMessages::UsePotion message;
    session->HandleUsePotion(message);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), 1);
    EXPECT_EQ(session->GetStats()->GetMana(), 0);
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 0.0f);
}

TEST_F(GameSessionVitalsTest, AChangedRestoreFractionAppliesFromTheNextPotion)
{
    std::shared_ptr<GameSession> const session = Enter(2.0f);
    int32 const max = session->GetStats()->GetMaxHitpoints();
    GameMessages::UsePotion message;
    session->HandleUsePotion(message);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), 1 + (max + 1) / 2);
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 1.0f);

    ASSERT_TRUE(session->SetHealth(1));
    ASSERT_TRUE(sSettings.Set("Potion.RestoreFraction", "1", SettingAuthor{ "test", 0, "test" }, "a full potion").Ok());
    session->HandleUsePotion(message);
    EXPECT_EQ(session->GetStats()->GetHitpoints(), max) << "the new fraction applies without a restart";
    EXPECT_EQ(session->GetStats()->GetMana(), session->GetStats()->GetMaxMana());
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 0.0f);
}

TEST_F(GameSessionVitalsTest, GoldAndVitalsStayWithinTheirMaximumsAndThePotionRefills)
{
    std::shared_ptr<GameSession> const session = Enter(1.0f);
    std::optional<GoldChange> const gold = session->ModifyGold(int64{ session->GetStats()->GetGoldPouch() } + 7);
    ASSERT_TRUE(gold);
    EXPECT_EQ(gold->Gold, session->GetStats()->GetGoldPouch());
    EXPECT_EQ(gold->Overflow, 7);
    ASSERT_TRUE(session->SetMana(1 << 30));
    EXPECT_EQ(session->GetStats()->GetMana(), session->GetStats()->GetMaxMana());

    auto const start = std::chrono::steady_clock::now();
    GameSessionVitalsTestAccess::RefillPotion(*session, start);
    GameSessionVitalsTestAccess::RefillPotion(*session, start + std::chrono::seconds(59));
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 1.0f);
    GameSessionVitalsTestAccess::RefillPotion(*session, start + std::chrono::seconds(60));
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 2.0f);
    GameSessionVitalsTestAccess::RefillPotion(*session, start + std::chrono::seconds(120));
    GameSessionVitalsTestAccess::RefillPotion(*session, start + std::chrono::seconds(180));
    GameSessionVitalsTestAccess::RefillPotion(*session, start + std::chrono::seconds(240));
    EXPECT_EQ(session->GetStats()->GetPotionCharge(), 3.0f) << "no more than the potion's maximum";
}
