/*
 * Project Ambrose by Imjustchico
 * Tests the beat a gameserver says it is alive with: configuring writes one row at once rather than after a wait, the tick counts up to the interval and writes again, a tick shorter than the interval writes nothing, a server naming no realm never beats at all, and stopping marks the realm offline rather than erasing when it was last alive, so the realm is offline the moment the server goes while the fact of when it last beat survives it.
 */

#include "RealmHeartbeat.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace
{
    struct Written
    {
        std::string Realm;
        uint32 Population = 0;
        int64 Heartbeat = 0;
        bool Online = false;
    };

    RealmHeartbeatSettings Settings(std::string realm, uint32 seconds = 30)
    {
        RealmHeartbeatSettings settings;
        settings.RealmName = std::move(realm);
        settings.IntervalSeconds = seconds;
        return settings;
    }
}

TEST(RealmHeartbeatTest, ConfiguringBeatsAtOnceAndTheTickBeatsAgainOnTheInterval)
{
    std::vector<Written> rows;
    uint32 population = 7;
    RealmHeartbeat heartbeat;
    heartbeat.Configure(Settings("Ambrose", 30), [&rows](std::string const& realm, uint32 count, int64 beat, bool online) { rows.push_back({ realm, count, beat, online }); },
        [&population] { return population; });

    ASSERT_EQ(rows.size(), 1u) << "a realm is reachable from the moment it can serve, not one interval later";
    EXPECT_EQ(rows[0].Realm, "Ambrose");
    EXPECT_EQ(rows[0].Population, 7u);
    EXPECT_TRUE(heartbeat.Beating());

    heartbeat.Update(29s);
    EXPECT_EQ(rows.size(), 1u) << "the beat waits for its whole interval";

    population = 9;
    heartbeat.Update(1s);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[1].Population, 9u) << "each beat carries the count as it is now, not as it was";

    heartbeat.Update(30s);
    EXPECT_EQ(rows.size(), 3u);
    EXPECT_EQ(heartbeat.Beats(), 3u);
}

TEST(RealmHeartbeatTest, AServerNamingNoRealmNeverBeats)
{
    std::vector<Written> rows;
    RealmHeartbeat heartbeat;
    heartbeat.Configure(Settings("", 30), [&rows](std::string const& realm, uint32 count, int64 beat, bool online) { rows.push_back({ realm, count, beat, online }); }, [] { return 0u; });

    EXPECT_FALSE(heartbeat.Beating());
    heartbeat.Update(300s);
    EXPECT_TRUE(rows.empty()) << "a server in no realm list is not one a player can be sent to";
    EXPECT_EQ(heartbeat.Beats(), 0u);
}

TEST(RealmHeartbeatTest, StoppingSaysSoAtOnceRatherThanGoingQuiet)
{
    std::vector<Written> rows;
    RealmHeartbeat heartbeat;
    heartbeat.Configure(Settings("Ambrose", 30), [&rows](std::string const& realm, uint32 count, int64 beat, bool online) { rows.push_back({ realm, count, beat, online }); }, [] { return 12u; });
    ASSERT_EQ(rows.size(), 1u);

    heartbeat.Stop();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[1].Population, 0u) << "a server that has gone holds nobody";
    EXPECT_FALSE(rows[1].Online) << "and marks the realm offline, so the login server sends nobody to it at once rather than after its missed beats";
    EXPECT_GT(rows[1].Heartbeat, 0) << "while when it was last alive stays true rather than being erased";
    EXPECT_TRUE(rows[0].Online) << "a live beat says the realm is online";
    EXPECT_FALSE(heartbeat.Beating());

    heartbeat.Update(300s);
    EXPECT_EQ(rows.size(), 2u) << "a stopped beat stays stopped";
    heartbeat.Stop();
    EXPECT_EQ(rows.size(), 2u) << "stopping twice writes one row, not two";
}
