/*
 * Project Ambrose by Imjustchico
 * Verifies the WizCombat wire declarations and the game session's in-world combat dispatch over loopback.
 */

#include "GameTestHarness.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "TestAppender.h"
#include "TestAppenderStore.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace
{
    class CapturedLog
    {
    public:
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };
}

TEST(WizCombatMessagesTest, CombatMoveAndSpectatorPhaseRoundTripTheirWireFields)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_NE(catalog, nullptr);

    GameMessages::CombatMove move;
    move.MoveType = 1;
    move.SpellSelection = 2;
    move.SpellTarget = 0x12345678;
    move.TimeLeft = -77;
    move.ShadowPactTarget = 19;
    move.SelectedTieredSpellId = 9001;

    ByteBuffer moveBytes;
    catalog->Encode(move, moveBytes);
    GameMessages::CombatMove decodedMove;
    ASSERT_EQ(catalog->Decode(std::span<uint8 const>(moveBytes.GetData()), decodedMove), MessageDecodeStatus::Ok);
    EXPECT_EQ(decodedMove.MoveType, move.MoveType);
    EXPECT_EQ(decodedMove.SpellSelection, move.SpellSelection);
    EXPECT_EQ(decodedMove.SpellTarget, move.SpellTarget);
    EXPECT_EQ(decodedMove.TimeLeft, move.TimeLeft);
    EXPECT_EQ(decodedMove.ShadowPactTarget, move.ShadowPactTarget);
    EXPECT_EQ(decodedMove.SelectedTieredSpellId, move.SelectedTieredSpellId);

    ByteBuffer reencodedMove;
    catalog->Encode(decodedMove, reencodedMove);
    EXPECT_TRUE(std::equal(reencodedMove.GetData().begin(), reencodedMove.GetData().end(), moveBytes.GetData().begin(), moveBytes.GetData().end()));

    GameMessages::CombatPhaseForSpectators phase;
    phase.DuelId = 0x1122334455667788;
    phase.NewPhase = 4;
    phase.Time = 30;
    phase.ParticipantName1 = "A";
    phase.ParticipantName2 = "B";
    phase.ParticipantName3 = "C";
    phase.ParticipantName4 = "D";
    phase.ParticipantName5 = "E";
    phase.ParticipantName6 = "F";
    phase.ParticipantName7 = "G";
    phase.ParticipantName8 = "H";
    phase.Subcircles = 0x1234;
    phase.TeamName0 = 17;
    phase.TeamName1 = 29;

    ByteBuffer phaseBytes;
    catalog->Encode(phase, phaseBytes);
    GameMessages::CombatPhaseForSpectators decodedPhase;
    ASSERT_EQ(catalog->Decode(std::span<uint8 const>(phaseBytes.GetData()), decodedPhase), MessageDecodeStatus::Ok);
    EXPECT_EQ(decodedPhase.DuelId, phase.DuelId);
    EXPECT_EQ(decodedPhase.NewPhase, phase.NewPhase);
    EXPECT_EQ(decodedPhase.Time, phase.Time);
    EXPECT_EQ(decodedPhase.ParticipantName1, phase.ParticipantName1);
    EXPECT_EQ(decodedPhase.ParticipantName2, phase.ParticipantName2);
    EXPECT_EQ(decodedPhase.ParticipantName3, phase.ParticipantName3);
    EXPECT_EQ(decodedPhase.ParticipantName4, phase.ParticipantName4);
    EXPECT_EQ(decodedPhase.ParticipantName5, phase.ParticipantName5);
    EXPECT_EQ(decodedPhase.ParticipantName6, phase.ParticipantName6);
    EXPECT_EQ(decodedPhase.ParticipantName7, phase.ParticipantName7);
    EXPECT_EQ(decodedPhase.ParticipantName8, phase.ParticipantName8);
    EXPECT_EQ(decodedPhase.Subcircles, phase.Subcircles);
    EXPECT_EQ(decodedPhase.TeamName0, phase.TeamName0);
    EXPECT_EQ(decodedPhase.TeamName1, phase.TeamName1);

    ByteBuffer reencodedPhase;
    catalog->Encode(decodedPhase, reencodedPhase);
    EXPECT_TRUE(std::equal(reencodedPhase.GetData().begin(), reencodedPhase.GetData().end(), phaseBytes.GetData().begin(), phaseBytes.GetData().end()));
}

TEST(WizCombatMessagesTest, CombatMoveRequiresInWorldAndLogsItsDecodedWireFields)
{
    GameTesting::GameDefinitions definitions;
    CapturedLog log;
    GameTesting::GameListener server;
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> const client = server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = server.Find(sessionId); return session != nullptr; }));
    ASSERT_EQ(session->GetStatus(), SessionStatus::Connected);

    GameMessages::CombatMove move;
    move.MoveType = 0;
    move.SpellSelection = 3;
    move.SpellTarget = 41;
    move.TimeLeft = 12;
    move.ShadowPactTarget = 5;
    move.SelectedTieredSpellId = 700;

    GameTesting::Send(*client, move);
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("received MSG_COMBATMOVE in state Connected, which needs InWorld"); }));
    EXPECT_FALSE(log.Contains("decoded MSG_COMBATMOVE"));
    EXPECT_EQ(session->GetStrikes(), 0u);
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);

    session->SetStatus(SessionStatus::InWorld);
    GameTesting::Send(*client, move);
    ASSERT_TRUE(WaitForCondition([&] { return log.Contains("decoded MSG_COMBATMOVE"); }));
    EXPECT_TRUE(log.Contains("MoveType 0, SpellSelection 3, SpellTarget 41, TimeLeft 12, ShadowPactTarget 5, SelectedTieredSpellID 700"));
    EXPECT_EQ(session->GetStrikes(), 0u);
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
}
