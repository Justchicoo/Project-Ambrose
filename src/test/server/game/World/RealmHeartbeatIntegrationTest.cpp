/*
 * Project Ambrose by Imjustchico
 * Two disabled tests the app smoke script runs by name around a real gameserver, because what 4.03 promises cannot be shown by a test that writes the rows itself. The first puts a realm with no heartbeat into the login database the server is about to beat for, so whatever is there afterwards is the server's and not a leftover. The second reads the row back and says whether the server beat while it ran, which its heartbeat still shows because stopping marks the realm offline rather than erasing when it was last alive, and whether stopping made it offline. They take the database from AMBROSE_TEST_DB, the realm from AMBROSE_REALM_NAME and the window a beat must land in from AMBROSE_REALM_MAX_AGE, so the script decides all three and the test only measures.
 */

#include "DBUpdater.h"
#include "Environment.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

namespace
{
    constexpr uint32 OfflineFlag = 1;
    constexpr int64 DefaultWindowSeconds = 120;

    std::optional<MySQLConnectionInfo> TestDatabase()
    {
        std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        return MySQLConnectionInfo::Parse(*text);
    }

    std::string RealmName()
    {
        std::optional<std::string> const name = Ambrose::GetEnv("AMBROSE_REALM_NAME");
        return name && !name->empty() ? *name : std::string("Ambrose");
    }

    int64 NowEpoch()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

TEST(RealmHeartbeatIntegration, DISABLED_AddTheRealmTheServerWillBeatFor)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";

    MySQLConnection connection(*info);
    ASSERT_EQ(connection.Open(), 0u);

    std::string const realm = RealmName();
    ASSERT_TRUE(connection.Execute(fmt::format(
        "INSERT INTO `realmlist` (`name`, `address`, `local_address`, `port`, `flags`, `population`, `player_limit`, `last_heartbeat`) "
        "VALUES ('{}', '127.0.0.1', '127.0.0.1', 12000, {}, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE `last_heartbeat` = 0, `population` = 0, `flags` = {}",
        realm, OfflineFlag, OfflineFlag))) << "the realmlist table 4.03 adds must exist by the time the gameserver has run its updates";

    QueryResult const check = connection.Query(fmt::format("SELECT `last_heartbeat`, `flags` FROM `realmlist` WHERE `name` = '{}'", realm));
    ASSERT_TRUE(check);
    EXPECT_EQ((*check)[0].Get<uint64>(), 0u) << "the realm starts with no heartbeat, so a beat afterwards is this server's";
    EXPECT_EQ((*check)[1].Get<uint32>() & OfflineFlag, OfflineFlag) << "and starts offline, so being online afterwards is this server's doing";
}

TEST(RealmHeartbeatIntegration, DISABLED_TheRealmBeatWhileItRanAndIsOfflineNow)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";

    MySQLConnection connection(*info);
    ASSERT_EQ(connection.Open(), 0u);

    std::string const realm = RealmName();
    QueryResult const row = connection.Query(fmt::format("SELECT `last_heartbeat`, `population`, `flags` FROM `realmlist` WHERE `name` = '{}'", realm));
    ASSERT_TRUE(row) << "the realm the server was told to be is gone from realmlist";

    int64 const heartbeat = static_cast<int64>((*row)[0].Get<uint64>());
    uint32 const population = (*row)[1].Get<uint32>();
    uint32 const flags = (*row)[2].Get<uint32>();

    std::optional<std::string> const given = Ambrose::GetEnv("AMBROSE_REALM_MAX_AGE");
    int64 const window = given ? Ambrose::StringTo<int64>(*given).value_or(DefaultWindowSeconds) : DefaultWindowSeconds;

    EXPECT_GT(heartbeat, 0) << "the server wrote no heartbeat while it ran, so no player would have been sent to it";
    EXPECT_LE(NowEpoch() - heartbeat, window) << "the heartbeat the server wrote is older than the window a live realm is allowed";
    EXPECT_EQ(flags & OfflineFlag, OfflineFlag) << "a server that has stopped leaves its realm offline, so the login server sends nobody to it";
    EXPECT_EQ(population, 0u) << "and holds nobody";
}
