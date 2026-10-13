/*
 * Project Ambrose by Imjustchico
 * Tests for the power operation locks, targets, disable records and protected hours of milestone 17.27.
 */

#include "Panel/PanelPower.h"

#include <gtest/gtest.h>

TEST(PanelPowerTest, SecondAppLockIsRefusedNamingHolderActionAndStart)
{
    PanelPower power;
    PowerOperationLock existing;
    ASSERT_TRUE(power.TryAcquireAppLock("loginserver", "alice", "restart", "loginserver", existing));
    PowerOperationLock refused;
    ASSERT_FALSE(power.TryAcquireAppLock("loginserver", "bob", "stop", "loginserver", refused));
    EXPECT_EQ(refused.Holder, "alice");
    EXPECT_EQ(refused.Action, "restart");
    EXPECT_GT(refused.StartEpochMs, 0);
    power.ReleaseAppLock("loginserver");
    PowerOperationLock fresh;
    EXPECT_TRUE(power.TryAcquireAppLock("loginserver", "bob", "stop", "loginserver", fresh));
}

TEST(PanelPowerTest, StackLockRefusedWhileAppLockHeld)
{
    PanelPower power;
    PowerOperationLock existing;
    ASSERT_TRUE(power.TryAcquireAppLock("worldserver", "alice", "restart", "worldserver", existing));
    PowerOperationLock refused;
    EXPECT_FALSE(power.TryAcquireStackLock("bob", "restart", "stack", refused));
    EXPECT_EQ(refused.Holder, "alice");
}

TEST(PanelPowerTest, AppLockRefusedWhileStackLockHeld)
{
    PanelPower power;
    PowerOperationLock existing;
    ASSERT_TRUE(power.TryAcquireStackLock("alice", "restart", "stack", existing));
    PowerOperationLock refused;
    EXPECT_FALSE(power.TryAcquireAppLock("loginserver", "bob", "start", "loginserver", refused));
    EXPECT_EQ(refused.Holder, "alice");
    EXPECT_EQ(refused.Target, "stack");
}

TEST(PanelPowerTest, KillBypassesHeldAppLock)
{
    PanelPower power;
    PowerOperationLock existing;
    ASSERT_TRUE(power.TryAcquireAppLock("worldserver", "alice", "restart", "worldserver", existing));
    EXPECT_TRUE(power.KillBypassAppLock("worldserver", "bob"));
    PowerOperationLock refused;
    EXPECT_FALSE(power.TryAcquireAppLock("worldserver", "carol", "stop", "worldserver", refused));
    EXPECT_EQ(refused.Holder, "bob");
    EXPECT_EQ(refused.Action, "kill");
}

TEST(PanelPowerTest, OperationLifecycleTracksStepsAndResult)
{
    PanelPower power;
    std::string id = power.BeginOperation(PowerTargetKind::Stack, "stack", "restart", "alice");
    EXPECT_FALSE(id.empty());
    power.AddStep(id, "stopping gameservers");
    power.AddStep(id, "stopping loginserver");
    power.FinishOperation(id, true, "stack restarted");
    std::optional<PowerOperation> found = power.FindOperation(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->State, PowerOperationState::Done);
    EXPECT_EQ(found->Steps.size(), 2u);
    EXPECT_EQ(found->Result, "stack restarted");
    EXPECT_TRUE(found->EndEpochMs.has_value());
}

TEST(PanelPowerTest, DisableNeedsReasonAndRefusesStart)
{
    PanelPower power;
    std::string error;
    EXPECT_FALSE(power.DisableApp("worldserver", "", "alice", error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(power.DisableApp("worldserver", "maintenance window", "alice", error));
    EXPECT_TRUE(power.IsDisabled("worldserver"));
    std::optional<PowerDisableRecord> info = power.DisableInfo("worldserver");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->Reason, "maintenance window");
    EXPECT_EQ(info->SetBy, "alice");
    EXPECT_TRUE(power.EnableApp("worldserver", error));
    EXPECT_FALSE(power.IsDisabled("worldserver"));
    EXPECT_FALSE(power.EnableApp("worldserver", error));
}

TEST(PanelPowerTest, ProtectedHoursParseAndMatch)
{
    uint8 from = 0, to = 0;
    std::string error;
    ASSERT_TRUE(PanelPower::ParseProtectedHours("02-06", from, to, error));
    EXPECT_EQ(from, 2);
    EXPECT_EQ(to, 6);
    EXPECT_TRUE(PanelPower::InProtectedHours(2, 6, 3));
    EXPECT_FALSE(PanelPower::InProtectedHours(2, 6, 7));
    EXPECT_TRUE(PanelPower::InProtectedHours(22, 2, 23));
    EXPECT_TRUE(PanelPower::InProtectedHours(22, 2, 1));
    EXPECT_FALSE(PanelPower::InProtectedHours(22, 2, 12));
    EXPECT_FALSE(PanelPower::ParseProtectedHours("nope", from, to, error));
    EXPECT_FALSE(PanelPower::ParseProtectedHours("25-06", from, to, error));
}

TEST(PanelPowerTest, ProtectedStatesAreNamed)
{
    EXPECT_TRUE(PanelPower::IsProtectedState("setup"));
    EXPECT_TRUE(PanelPower::IsProtectedState("updating"));
    EXPECT_TRUE(PanelPower::IsProtectedState("restoring"));
    EXPECT_TRUE(PanelPower::IsProtectedState("moving"));
    EXPECT_FALSE(PanelPower::IsProtectedState("running"));
    EXPECT_FALSE(PanelPower::IsProtectedState("offline"));
}

TEST(PanelPowerTest, TargetKindNames)
{
    EXPECT_EQ(PanelPower::TargetKindName(PowerTargetKind::App), "app");
    EXPECT_EQ(PanelPower::TargetKindName(PowerTargetKind::Realm), "realm");
    EXPECT_EQ(PanelPower::TargetKindName(PowerTargetKind::Stack), "stack");
}
