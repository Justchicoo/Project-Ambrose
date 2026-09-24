/*
 * Project Ambrose by Imjustchico
 * Tests which realm a player is sent to: one whose heartbeat is older than the missed runs the policy allows is gone from the list, one marked offline by hand is gone whatever its heartbeat says, and one that has never beaten at all is not treated as alive. Choosing reads the named realm first, the operator's default second and the least full of the rest last, a named realm that is down is refused rather than replaced by another, a full realm is not chosen by the least-full rule though it can still be asked for by name, and when nothing is online the answer is none rather than any.
 */

#include "RealmList.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr int64 Now = 1'700'000'000;

    Realm Make(uint32 id, std::string name, uint32 population, int64 heartbeat, uint32 flags = REALM_FLAG_NONE, uint32 limit = 0)
    {
        Realm realm;
        realm.Id = id;
        realm.Name = std::move(name);
        realm.Address = "127.0.0.1";
        realm.LocalAddress = "127.0.0.1";
        realm.Port = static_cast<uint16>(12000 + id);
        realm.Flags = flags;
        realm.Population = population;
        realm.PlayerLimit = limit;
        realm.LastHeartbeatEpoch = heartbeat;
        return realm;
    }

    RealmPolicy Policy(std::string defaultRealm = {})
    {
        RealmPolicy policy;
        policy.HeartbeatSeconds = 30;
        policy.OfflineAfterIntervals = 3;
        policy.DefaultRealm = std::move(defaultRealm);
        return policy;
    }
}

TEST(RealmListTest, AHeartbeatOlderThanTheAllowedMissedRunsIsOffline)
{
    RealmPolicy const policy = Policy();
    EXPECT_EQ(policy.OldestLiveHeartbeat(Now), Now - 90);

    EXPECT_TRUE(RealmList::IsOnline(Make(1, "Ambrose", 0, Now), policy, Now));
    EXPECT_TRUE(RealmList::IsOnline(Make(1, "Ambrose", 0, Now - 89), policy, Now))
        << "a realm that missed two beats is still expected to answer the third";
    EXPECT_TRUE(RealmList::IsOnline(Make(1, "Ambrose", 0, Now - 90), policy, Now))
        << "the oldest live heartbeat is itself still alive";
    EXPECT_FALSE(RealmList::IsOnline(Make(1, "Ambrose", 0, Now - 91), policy, Now));

    EXPECT_FALSE(RealmList::IsOnline(Make(1, "Ambrose", 0, Now, REALM_FLAG_OFFLINE), policy, Now))
        << "an operator who marked a realm down is not overruled by its own heartbeat";
    EXPECT_FALSE(RealmList::IsOnline(Make(1, "Ambrose", 0, 0), policy, Now))
        << "a realm that has never said anything has not said it is alive";
}

TEST(RealmListTest, TheNamedRealmIsChosenAndADownOneIsRefusedRatherThanReplaced)
{
    std::vector<Realm> const realms{ Make(1, "Ambrose", 50, Now), Make(2, "Wysteria", 5, Now), Make(3, "Marleybone", 1, Now - 600) };

    std::optional<Realm> const named = RealmList::Choose(realms, Policy(), "Wysteria", Now);
    ASSERT_TRUE(named.has_value());
    EXPECT_EQ(named->Id, 2u);

    EXPECT_TRUE(RealmList::Choose(realms, Policy(), "wysteria", Now).has_value()) << "a realm name is matched the way a player types it";

    EXPECT_FALSE(RealmList::Choose(realms, Policy(), "Marleybone", Now).has_value())
        << "a player who asked for a world that is down is told so, not moved to another";
    EXPECT_FALSE(RealmList::Choose(realms, Policy(), "Nowhere", Now).has_value());
}

TEST(RealmListTest, WithoutANameTheDefaultComesFirstAndTheLeastFullLast)
{
    std::vector<Realm> const realms{ Make(1, "Ambrose", 50, Now), Make(2, "Wysteria", 5, Now) };

    std::optional<Realm> const byDefault = RealmList::Choose(realms, Policy("Ambrose"), "", Now);
    ASSERT_TRUE(byDefault.has_value());
    EXPECT_EQ(byDefault->Id, 1u) << "the operator's default is taken before the least-full rule";

    std::optional<Realm> const leastFull = RealmList::Choose(realms, Policy(), "", Now);
    ASSERT_TRUE(leastFull.has_value());
    EXPECT_EQ(leastFull->Id, 2u);

    std::optional<Realm> const defaultIsDown = RealmList::Choose(realms, Policy("Marleybone"), "", Now);
    ASSERT_TRUE(defaultIsDown.has_value());
    EXPECT_EQ(defaultIsDown->Id, 2u) << "a default that is not there falls through to the least full rather than refusing";
}

TEST(RealmListTest, AFullRealmIsNotChosenByTheLeastFullRuleAndNoneOnlineAnswersNone)
{
    std::vector<Realm> const full{ Make(1, "Ambrose", 100, Now, REALM_FLAG_NONE, 100), Make(2, "Wysteria", 20, Now, REALM_FLAG_NONE, 20) };
    EXPECT_FALSE(RealmList::Choose(full, Policy(), "", Now).has_value());
    EXPECT_TRUE(RealmList::Choose(full, Policy(), "Ambrose", Now).has_value())
        << "a full realm is still the one a player asked for, and the fullness is answered where the seat is taken";

    std::vector<Realm> const down{ Make(1, "Ambrose", 0, Now - 600), Make(2, "Wysteria", 0, Now, REALM_FLAG_OFFLINE) };
    EXPECT_FALSE(RealmList::Choose(down, Policy(), "", Now).has_value());
    EXPECT_FALSE(RealmList::Choose(down, Policy("Ambrose"), "", Now).has_value());
    EXPECT_FALSE(RealmList::Choose({}, Policy(), "", Now).has_value());
}

TEST(RealmListTest, TheStoreKeepsWhatItWasGivenAndReadsItBackByPolicy)
{
    RealmList& realms = sRealmList;
    realms.SetPolicy(Policy("Ambrose"));
    realms.Replace({ Make(1, "Ambrose", 50, Now), Make(2, "Wysteria", 5, Now - 600) });

    EXPECT_EQ(realms.All().size(), 2u);
    EXPECT_EQ(realms.Online(Now).size(), 1u) << "reading the list back applies the same heartbeat rule";
    ASSERT_TRUE(realms.Find("ambrose").has_value());
    EXPECT_EQ(realms.Find("ambrose")->Port, 12001);
    EXPECT_FALSE(realms.Find("Nowhere").has_value());

    std::optional<Realm> const chosen = realms.Choose("", Now);
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->Id, 1u);

    realms.Replace({});
    EXPECT_TRUE(realms.All().empty());
}
