/*
 * Project Ambrose by Imjustchico
 * Tests what a game session sends of its instance's walker changes: an object snapped to a node is sent with MSG_SERVERTELEPORT and its move state, a state a walker enters is sent with MSG_ENTERSTATE carrying its data, both only for objects the wizard sees, and a zone event a walker posts fires the wizard's own triggers in its zone and shows their notify text.
 */

#include "GameTestHarness.h"
#include "PathWalkers.h"
#include "ZoneTriggerMgr.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

struct GameSessionObjectChangesTestAccess
{
    static void StandInZone(GameSession& session, uint32 mapId, std::string zone, uint64 wizard, uint64 sees)
    {
        session._mapId = mapId;
        session._zonePath = std::move(zone);
        session._worldGuid = wizard;
        session._sight.Update({ VisibilityCandidate{ sees, 0.0f, false } }, VisibilityRange{});
    }
};

namespace
{
    using namespace GameTesting;

    constexpr char const* Zone = "WizardCity/WC_Streets/WC_Unicorn";
    constexpr uint64 Ghost = 1001;
    constexpr uint64 Unseen = 1002;

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

    class GameSessionObjectChangesTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ZoneTrigger trigger;
            trigger.Index = 0;
            trigger.Name = "Trigger Ghost Arrives";
            trigger.TriggerMax = 1;
            trigger.FireEvents = { "TEST1" };
            trigger.NotifyTexts = { { "WizardPOI_00000001", 1 } };
            ZoneTriggerData data;
            data.Triggers.push_back(std::move(trigger));
            std::map<std::string, ZoneTriggerData, std::less<>> zones;
            zones.emplace(Zone, std::move(data));
            sZoneTriggerMgr.Replace(std::move(zones));
        }

        void TearDown() override
        {
            sZoneTriggerMgr.Clear();
        }

        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(GameSessionObjectChangesTest, ASnappedWalkerItsStatesAndItsZoneEventsReachTheWizardWhoSeesIt)
{
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> client = _server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
    session->SetStatus(SessionStatus::InWorld);
    GameSessionObjectChangesTestAccess::StandInZone(*session, 1, Zone, 7, Ghost);

    MapObjectChanges changes;
    changes.DynamicZoneId = 1;
    changes.Moved.push_back({ Unseen, 40001, { 5.0f, 0.0f, 0.0f }, 0.0f, PathWalkers::WalkingState, true });
    changes.Moved.push_back({ Ghost, 40000, { 0.0f, 0.0f, 0.0f }, 0.0f, PathWalkers::StandingState, true });
    changes.States.push_back({ Unseen, PathWalkers::EmotingState, "unseen" });
    changes.States.push_back({ Ghost, PathWalkers::EmotingState, "jump-override" });
    changes.ZoneEvents.push_back("TEST1");
    session->SendObjectChanges(changes);

    std::optional<GameMessages::ServerTeleport> const teleport = ReadReply<GameMessages::ServerTeleport>(*client);
    ASSERT_TRUE(teleport) << "the walker that was snapped to a node is teleported, not walked there";
    EXPECT_EQ(teleport->MobileId, 40000);
    std::optional<GameMessages::MoveState> const standing = ReadReply<GameMessages::MoveState>(*client);
    ASSERT_TRUE(standing);
    EXPECT_EQ(standing->GlobalId, Ghost);
    EXPECT_EQ(standing->NewState, PathWalkers::StandingState);
    std::optional<GameMessages::EnterState> const emoting = ReadReply<GameMessages::EnterState>(*client);
    ASSERT_TRUE(emoting) << "the object the wizard cannot see sends nothing";
    EXPECT_EQ(emoting->GameObjectId, Ghost);
    EXPECT_EQ(emoting->State, PathWalkers::EmotingState);
    EXPECT_EQ(emoting->Data, "jump-override");
    std::optional<GameMessages::ClientNotifyText> const notify = ReadReply<GameMessages::ClientNotifyText>(*client);
    ASSERT_TRUE(notify) << "the walker's zone event fires the wizard's trigger";
    EXPECT_EQ(notify->NotifyText, "WizardPOI_00000001");
    EXPECT_EQ(notify->Type, 1);
}

TEST_F(GameSessionObjectChangesTest, AZoneEventWaitsForAWizardStandingInTheWorld)
{
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> client = _server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
    session->SetStatus(SessionStatus::LoggedIn);
    GameSessionObjectChangesTestAccess::StandInZone(*session, 1, Zone, 7, Ghost);

    MapObjectChanges changes;
    changes.DynamicZoneId = 1;
    changes.ZoneEvents.push_back("TEST1");
    session->SendObjectChanges(changes);
    session->SetStatus(SessionStatus::InWorld);
    session->SendObjectChanges(changes);
    std::optional<GameMessages::ClientNotifyText> const notify = ReadReply<GameMessages::ClientNotifyText>(*client);
    ASSERT_TRUE(notify) << "only the second event, posted once the wizard stands in the world, fires";
    EXPECT_TRUE(sZoneTriggerMgr.Post(1, Zone, "TEST1", 7, ZoneTriggers::Clock::now()).empty()) << "the trigger fires once, and its one firing was the event posted in the world";
}
