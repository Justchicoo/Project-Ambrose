/*
 * Project Ambrose by Imjustchico
 * Disabled tests the app smoke scripts run by name to prepare pending world updates and drop the databases they created, refusing any name outside their prefixes.
 */

#include "DBUpdater.h"
#include "Environment.h"
#include "MySQLConnection.h"
#include "StringUtil.h"
#include "TestDatabaseUpdates.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

TEST(TestDatabaseSetup, DISABLED_ApplyWorldUpdates)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> const info = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(info);
    ASSERT_TRUE(AmbroseTestDatabase::RunWorldUpdates(*info));
}

TEST(TestDatabaseCleanup, DISABLED_DropNamedDatabases)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<std::string> const names = Ambrose::GetEnv("AMBROSE_DROP_DATABASES");
    if (!names || names->empty())
        GTEST_SKIP() << "AMBROSE_DROP_DATABASES is not set";
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(info);
    info->Database.clear();
    MySQLConnection connection(*info);
    ASSERT_EQ(connection.Open(), 0u);
    for (std::string_view const name : Ambrose::Tokenize(*names, ',', false))
    {
        ASSERT_TRUE(name.starts_with("ambrose_smoke_") || name.starts_with("ambrose_dbimport_") || name.starts_with("ambrose_extractor_")) << "refusing to drop " << name;
        EXPECT_TRUE(connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(std::string(name))))) << name;
    }
}
