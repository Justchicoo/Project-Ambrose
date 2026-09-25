/*
 * Project Ambrose by Imjustchico
 * Tests the zone instances and the ids their objects carry, with a clock and settings the test holds: two instances of one zone get different dynamic zone ids while the public one is found again by its path, an instance that empties is taken down only once its delay has passed and a wizard who comes back first keeps it alive, the delay is the one in force when the instance empties so a changed Zone.UnloadDelay applies to the next one with nothing restarted, a wizard keeps one mobile id however often it is added, the same placed object is given the same permID on every run while runtime GIDs are never repeated and never fall where a stored id could.
 */

#include "MapMgr.h"
#include "ObjectGuid.h"

#include <gtest/gtest.h>

#include <chrono>
#include <set>

namespace
{
    using Clock = MapMgr::Clock;

    class MapTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sMapMgr.Clear();
            sMapMgr.SetClock([this] { return _now; });
            sMapMgr.SetSettingsReader([this] { return _settings; });
        }

        void TearDown() override
        {
            sMapMgr.Clear();
        }

        Clock::time_point _now{};
        MapSettings _settings;
    };
}

TEST_F(MapTest, TwoInstancesOfOneZoneGetDifferentDynamicZoneIds)
{
    Map& shared = sMapMgr.FindOrCreatePublic("WizardCity/WC_Hub");
    Map& privately = sMapMgr.CreatePrivate("WizardCity/WC_Hub");
    EXPECT_NE(shared.GetDynamicZoneId(), privately.GetDynamicZoneId());
    EXPECT_NE(shared.GetDynamicZoneId(), 0u);
    EXPECT_EQ(&sMapMgr.FindOrCreatePublic("WizardCity/WC_Hub"), &shared) << "the public instance of a zone is found again by its path";
    EXPECT_EQ(sMapMgr.Find(privately.GetDynamicZoneId()), &privately);
    EXPECT_EQ(sMapMgr.GetMapCount(), 2u);
}

TEST_F(MapTest, AnEmptyInstanceIsTakenDownOnlyAfterItsDelay)
{
    _settings.UnloadDelay = std::chrono::seconds(60);
    Map& map = sMapMgr.FindOrCreatePublic("WizardCity/WC_Ravenwood");
    uint32 const id = map.GetDynamicZoneId();
    ASSERT_TRUE(sMapMgr.AddPlayer(map, 1));
    EXPECT_TRUE(sMapMgr.Update().empty()) << "an instance with somebody in it is never taken down";

    ASSERT_TRUE(sMapMgr.RemovePlayer(map, 1));
    _now += std::chrono::seconds(59);
    EXPECT_TRUE(sMapMgr.Update().empty()) << "not before its delay";
    EXPECT_NE(sMapMgr.Find(id), nullptr);

    _now += std::chrono::seconds(2);
    std::vector<uint32> const taken = sMapMgr.Update();
    ASSERT_EQ(taken.size(), 1u);
    EXPECT_EQ(taken.front(), id);
    EXPECT_EQ(sMapMgr.Find(id), nullptr);
    EXPECT_EQ(sMapMgr.FindPublic("WizardCity/WC_Ravenwood"), nullptr) << "and the next wizard to arrive is given a fresh instance";
}

TEST_F(MapTest, AWizardWhoComesBackBeforeTheDelayKeepsTheInstance)
{
    _settings.UnloadDelay = std::chrono::seconds(60);
    Map& map = sMapMgr.FindOrCreatePublic("WizardCity/WC_Hub");
    ASSERT_TRUE(sMapMgr.AddPlayer(map, 1));
    ASSERT_TRUE(sMapMgr.RemovePlayer(map, 1));
    _now += std::chrono::seconds(30);
    ASSERT_TRUE(sMapMgr.AddPlayer(map, 1));
    _now += std::chrono::seconds(60);
    EXPECT_TRUE(sMapMgr.Update().empty());
    EXPECT_EQ(sMapMgr.GetMapCount(), 1u);
}

TEST_F(MapTest, TheDelayInForceWhenAnInstanceEmptiesIsTheOneUsed)
{
    _settings.UnloadDelay = std::chrono::seconds(60);
    Map& first = sMapMgr.CreatePrivate("WizardCity/WC_Hub");
    ASSERT_TRUE(sMapMgr.AddPlayer(first, 1));
    ASSERT_TRUE(sMapMgr.RemovePlayer(first, 1));

    _settings.UnloadDelay = std::chrono::seconds(5);
    Map& second = sMapMgr.CreatePrivate("WizardCity/WC_Hub");
    ASSERT_TRUE(sMapMgr.AddPlayer(second, 2));
    ASSERT_TRUE(sMapMgr.RemovePlayer(second, 2));
    uint32 const firstId = first.GetDynamicZoneId();
    uint32 const secondId = second.GetDynamicZoneId();

    _now += std::chrono::seconds(6);
    std::vector<uint32> const taken = sMapMgr.Update();
    ASSERT_EQ(taken.size(), 1u) << "only the instance that emptied after the setting changed uses the new delay";
    EXPECT_EQ(taken.front(), secondId);
    EXPECT_NE(sMapMgr.Find(firstId), nullptr);
    EXPECT_EQ(sMapMgr.GetMapCount(), 1u);
}

TEST_F(MapTest, AWizardKeepsOneMobileIdHoweverOftenItIsAdded)
{
    Map& map = sMapMgr.FindOrCreatePublic("WizardCity/WC_Hub");
    std::optional<uint16> const first = sMapMgr.AddPlayer(map, 7);
    std::optional<uint16> const again = sMapMgr.AddPlayer(map, 7);
    ASSERT_TRUE(first);
    ASSERT_TRUE(again);
    EXPECT_EQ(*first, *again);
    EXPECT_GE(*first, MobileIdAllocator::FirstPlayerId) << "a wizard's id comes from the player range";
    EXPECT_EQ(map.GetPlayerCount(), 1u);
    EXPECT_EQ(map.GetMobileIds().Held(MobileIdAllocator::Range::Player), 1u);
}

TEST(ObjectGuidTest, TheSamePlacedObjectGetsTheSamePermIdOnEveryRun)
{
    uint64 const hub = ObjectGuid::PermId("WizardCity/WC_Hub", 4242, 7);
    EXPECT_EQ(hub, ObjectGuid::PermId("WizardCity/WC_Hub", 4242, 7)) << "nothing but the zone, the template and the object decide it";
    EXPECT_EQ(hub, 0x5658625877D119B9ull) << "the value is pinned, so a change to how it is worked out breaks every permID a live server has handed out";
    EXPECT_NE(hub, ObjectGuid::PermId("WizardCity/WC_Hub", 4242, 8));
    EXPECT_NE(hub, ObjectGuid::PermId("WizardCity/WC_Hub", 4243, 7));
    EXPECT_NE(hub, ObjectGuid::PermId("WizardCity/WC_Ravenwood", 4242, 7));
    EXPECT_NE(ObjectGuid::PermId("", 0, 0), 0u) << "zero is what an object with no permID carries";
}

TEST(ObjectGuidTest, RuntimeGidsAreNeverRepeatedAndNeverFallWhereAStoredIdCould)
{
    std::set<uint64> seen;
    for (int handed = 0; handed < 10000; ++handed)
    {
        std::optional<uint64> const guid = ObjectGuid::NextRuntime();
        ASSERT_TRUE(guid);
        EXPECT_TRUE(ObjectGuid::IsRuntime(*guid));
        EXPECT_GE(*guid, ObjectGuid::RuntimeBase);
        EXPECT_TRUE(seen.insert(*guid).second) << "runtime GID " << *guid << " was handed out twice";
    }
    EXPECT_FALSE(ObjectGuid::IsRuntime(1)) << "a wizard's stored id is never read as a runtime one";
}
