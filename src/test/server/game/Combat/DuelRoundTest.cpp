/*
 * Project Ambrose by Imjustchico
 * Tests a duel's round headless through DuelSimHarness: three rounds of a wizard and a creature passing give phases 1, 2, 4 and 5 in turn with rounds 1 to 3 and pips 1 to 3, Execution begins in the tick of the last move, a Combat.PlanningSeconds change made during round one leaves its timer and gives round two the new one, a timer that runs out passes for whoever has not moved, pips stop at the most a seat may hold, a change of mind withdraws a pass, and any other move, a move outside planning and one from outside the duel are refused.
 */

#include "ConfigMgr.h"
#include "DuelRound.h"
#include "DuelSimHarness.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Wizard = 0x100;
    constexpr uint64 Creature = 0x200;

    DuelRoundRules QuickRules()
    {
        DuelRoundRules rules;
        rules.StartGraceMs = 1000;
        rules.PrePlanningMs = 500;
        rules.PlanningMs = 5000;
        rules.PipsPerRound = 1;
        rules.MaxPips = 7;
        return rules;
    }

    bool InRound(DuelSimHarness const& sim, int32 round, DuelPhase phase)
    {
        return sim.GetRound().GetRound() == round && sim.GetRound().GetPhase() == phase;
    }

    class DuelRoundSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSettings.Clear();
            _file = _directory.Write("gameserver.conf", "Combat.StartGraceSeconds = 1\nCombat.PrePlanningSeconds = 1\nCombat.PlanningSeconds = 30\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_file).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override
        {
            sSettings.Clear();
        }

        LogTestDirectory _directory;
        std::filesystem::path _file;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST(DuelRoundTest, ThreePassingRoundsRepeatThePhasesAndGainAPipEach)
{
    DuelSimHarness sim(QuickRules());
    sim.AddPlayer(Wizard, DuelSimHarness::PassAfter(2));
    sim.AddCreature(Creature);
    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 4, DuelPhase::PrePlanning); }));

    std::vector<uint8> phases;
    for (DuelSimHarness::Record const& record : sim.RecordsOf(DuelEventKind::PhaseChanged))
        if (record.Event.Round <= 3)
            phases.push_back(static_cast<uint8>(record.Event.Phase));
    EXPECT_EQ(phases, (std::vector<uint8>{ 1, 2, 4, 5, 1, 2, 4, 5, 1, 2, 4, 5 }));

    std::vector<int32> rounds;
    for (DuelSimHarness::Record const& record : sim.RecordsOf(DuelEventKind::RoundStarted))
        rounds.push_back(record.Event.Round);
    EXPECT_EQ(rounds, (std::vector<int32>{ 1, 2, 3, 4 }));

    DuelSimHarness again(QuickRules());
    again.AddPlayer(Wizard, DuelSimHarness::PassAfter(2));
    again.AddCreature(Creature);
    std::vector<int32> pips;
    for (int32 round = 1; round <= 3; ++round)
    {
        ASSERT_TRUE(again.RunUntil([round](DuelSimHarness const& at) { return InRound(at, round, DuelPhase::PrePlanning); }));
        for (DuelSeat const& seat : again.GetRound().GetSeats())
        {
            EXPECT_EQ(seat.Pips, round) << seat.OwnerId;
        }
        pips.push_back(again.GetRound().FindSeat(Wizard)->Pips);
    }
    EXPECT_EQ(pips, (std::vector<int32>{ 1, 2, 3 }));
}

TEST(DuelRoundTest, ExecutionStartsInTheTickOfTheLastMove)
{
    DuelSimHarness sim(QuickRules(), 100, 7);
    sim.AddPlayer(Wizard, DuelSimHarness::PassAtRandom(20));
    sim.AddPlayer(Wizard + 1, DuelSimHarness::PassAtRandom(20));
    sim.AddCreature(Creature);
    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 3, DuelPhase::PrePlanning); }));

    for (int32 round = 1; round <= 2; ++round)
    {
        std::optional<DuelSimHarness::Record> const planning = sim.FirstPhase(DuelPhase::Planning, round);
        std::optional<DuelSimHarness::Record> const execution = sim.FirstPhase(DuelPhase::Execution, round);
        ASSERT_TRUE(planning && execution) << "round " << round;
        uint64 lastMove = 0;
        for (DuelSimHarness::Choice const& choice : sim.GetChoices())
            if (choice.Tick > planning->Tick && choice.Tick <= execution->Tick)
                lastMove = std::max(lastMove, choice.Tick);
        ASSERT_NE(lastMove, 0u) << "round " << round;
        EXPECT_LE(execution->Tick - lastMove, 1u) << "round " << round;
        EXPECT_LT(execution->TimeMs - planning->TimeMs, QuickRules().PlanningMs) << "the round ended on its moves, not its timer";
    }
}

TEST_F(DuelRoundSettingsTest, APlanningSecondsChangeAppliesFromTheNextRound)
{
    DuelSimHarness sim(DuelRoundRules{}, 100);
    sim.SetRules([] { return DuelRoundRules::FromSettings(); });
    sim.AddPlayer(Wizard, DuelSimHarness::Never());
    sim.AddCreature(Creature);

    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 1, DuelPhase::Planning); }));
    std::vector<DuelSimHarness::Record> const firstTimer = sim.RecordsOf(DuelEventKind::PlanningTimer);
    ASSERT_EQ(firstTimer.size(), 1u);
    EXPECT_EQ(firstTimer.front().Event.Seconds, 30u);

    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Combat.PlanningSeconds", "10", author, "shorter planning").Ok());

    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 2, DuelPhase::Execution); }));
    std::optional<DuelSimHarness::Record> const planningOne = sim.FirstPhase(DuelPhase::Planning, 1);
    std::optional<DuelSimHarness::Record> const executionOne = sim.FirstPhase(DuelPhase::Execution, 1);
    std::optional<DuelSimHarness::Record> const planningTwo = sim.FirstPhase(DuelPhase::Planning, 2);
    std::optional<DuelSimHarness::Record> const executionTwo = sim.FirstPhase(DuelPhase::Execution, 2);
    ASSERT_TRUE(planningOne && executionOne && planningTwo && executionTwo);
    EXPECT_EQ(executionOne->TimeMs - planningOne->TimeMs, 30000u) << "round one keeps the timer it began with";
    EXPECT_EQ(executionTwo->TimeMs - planningTwo->TimeMs, 10000u) << "round two takes the new timer";
    std::vector<DuelSimHarness::Record> const timers = sim.RecordsOf(DuelEventKind::PlanningTimer);
    ASSERT_EQ(timers.size(), 2u);
    EXPECT_EQ(timers.back().Event.Seconds, 10u);
}

TEST(DuelRoundTest, ATimerThatRunsOutPassesForWhoeverHasNotMoved)
{
    DuelSimHarness sim(QuickRules());
    sim.AddPlayer(Wizard, DuelSimHarness::Never());
    sim.AddCreature(Creature);
    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 1, DuelPhase::Execution); }));
    for (DuelSeat const& seat : sim.GetRound().GetSeats())
    {
        ASSERT_TRUE(seat.Move.has_value()) << seat.OwnerId;
        EXPECT_EQ(*seat.Move, DuelMove::Pass) << seat.OwnerId;
    }
    std::optional<DuelSimHarness::Record> const planning = sim.FirstPhase(DuelPhase::Planning, 1);
    std::optional<DuelSimHarness::Record> const execution = sim.FirstPhase(DuelPhase::Execution, 1);
    ASSERT_TRUE(planning && execution);
    EXPECT_EQ(execution->TimeMs - planning->TimeMs, QuickRules().PlanningMs);
}

TEST(DuelRoundTest, PipsStopAtTheMostASeatMayHold)
{
    DuelRoundRules rules = QuickRules();
    rules.PipsPerRound = 3;
    rules.MaxPips = 7;
    DuelSimHarness sim(rules);
    sim.AddPlayer(Wizard, DuelSimHarness::PassAfter(0));
    sim.AddCreature(Creature);
    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 4, DuelPhase::PrePlanning); }));
    EXPECT_EQ(sim.GetRound().FindSeat(Wizard)->Pips, 7);
    EXPECT_EQ(sim.GetRound().FindSeat(Creature)->Pips, 7);
}

TEST(DuelRoundTest, AChangeOfMindWithdrawsAPassAndOtherMovesAreRefused)
{
    DuelSimHarness sim(QuickRules());
    sim.AddPlayer(Wizard, DuelSimHarness::Never());
    sim.AddCreature(Creature);
    DuelRound& round = sim.GetRound();
    EXPECT_EQ(round.Choose(Wizard, static_cast<uint8>(DuelMove::Pass)), DuelMoveResult::NotPlanning);
    ASSERT_TRUE(sim.RunUntil([](DuelSimHarness const& at) { return InRound(at, 1, DuelPhase::Planning); }));

    EXPECT_EQ(round.Choose(0x999, static_cast<uint8>(DuelMove::Pass)), DuelMoveResult::NotSeated);
    EXPECT_EQ(round.Choose(Wizard, static_cast<uint8>(DuelMove::Cast)), DuelMoveResult::Unsupported);
    EXPECT_EQ(round.Choose(Wizard, 7), DuelMoveResult::Unsupported);
    EXPECT_FALSE(round.FindSeat(Wizard)->Move.has_value());

    EXPECT_EQ(round.Choose(Wizard, static_cast<uint8>(DuelMove::Pass)), DuelMoveResult::Chosen);
    ASSERT_TRUE(round.FindSeat(Wizard)->Move.has_value());
    EXPECT_EQ(round.Choose(Wizard, static_cast<uint8>(DuelMove::ChangeMind)), DuelMoveResult::Withdrawn);
    EXPECT_FALSE(round.FindSeat(Wizard)->Move.has_value());

    std::vector<DuelEvent> const& events = round.GetEvents();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].Kind, DuelEventKind::MoveChosen);
    EXPECT_EQ(events[0].Move, DuelMove::Pass);
    EXPECT_EQ(events[0].Team, 0);
    EXPECT_EQ(events[1].Move, DuelMove::ChangeMind);

    sim.Tick();
    EXPECT_EQ(round.GetPhase(), DuelPhase::Planning) << "a withdrawn pass keeps the round in planning";
}
