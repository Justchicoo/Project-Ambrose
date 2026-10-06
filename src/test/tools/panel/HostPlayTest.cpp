/*
 * Project Ambrose by Imjustchico
 * Checks hosting's two decisions: Play waits, saying for what, while the supervisor reports the login server absent, starting or running without a ready time, and once it is ready gives the launcher the local login server's address and port; and a private database runs only when every file its SHA256SUMS names matches, a changed, missing or escaping file being refused by name.
 */

#include "Hex.h"
#include "HostPlay.h"
#include "LogTestDirectory.h"
#include "SHA256.h"

#include <gtest/gtest.h>

#include <string>

TEST(HostPlayTest, PlayWaitsForTheLoginServerAndThenGivesTheLauncherItsAddress)
{
    PlayDecision const none = HostPlay::Decide(R"({"apps":[{"name":"gameserver","state":"running","ready_epoch_ms":1}]})", "127.0.0.1", 12000);
    EXPECT_FALSE(none.Ready);
    EXPECT_NE(none.Waiting.find("no login server"), std::string::npos) << none.Waiting;

    PlayDecision const starting = HostPlay::Decide(R"({"apps":[{"name":"loginserver","state":"starting","ready_epoch_ms":null,"start":{"stage":"building the type dump","until_ms":0}}]})", "127.0.0.1", 12000);
    EXPECT_FALSE(starting.Ready);
    EXPECT_NE(starting.Waiting.find("building the type dump"), std::string::npos) << starting.Waiting;

    PlayDecision const unready = HostPlay::Decide(R"({"apps":[{"name":"loginserver","state":"running","ready_epoch_ms":null}]})", "127.0.0.1", 12000);
    EXPECT_FALSE(unready.Ready);

    PlayDecision const ready = HostPlay::Decide(R"({"apps":[{"name":"loginserver","state":"running","ready_epoch_ms":1700000000000}]})", "127.0.0.1", 12010);
    ASSERT_TRUE(ready.Ready) << ready.Waiting;
    EXPECT_EQ(ready.LauncherArguments, (std::vector<std::string>{ "--host", "127.0.0.1", "--port", "12010" }));
}

TEST(HostPlayTest, APrivateDatabaseRunsOnlyWhenEveryFileMatchesItsPublishedChecksum)
{
    LogTestDirectory directory;
    std::filesystem::path const folder = directory.Path() / "mariadb";
    std::string const program = "mariadbd program bytes";
    std::string const library = "library bytes";
    directory.Write("mariadb/bin/mariadbd.exe", program);
    directory.Write("mariadb/lib/plugin.dll", library);
    auto const sum = [](std::string const& bytes) { return Hex::Encode(SHA256::GetDigestOf(std::string_view(bytes)), Hex::Case::Lower); };
    directory.Write("mariadb/SHA256SUMS", sum(program) + "  bin/mariadbd.exe\n" + sum(library) + " *lib/plugin.dll\n");
    std::string error;
    EXPECT_TRUE(HostPlay::VerifyDatabase(folder, error)) << error;

    directory.Write("mariadb/lib/plugin.dll", "changed");
    EXPECT_FALSE(HostPlay::VerifyDatabase(folder, error));
    EXPECT_NE(error.find("lib/plugin.dll"), std::string::npos) << error;
    EXPECT_NE(error.find("does not match"), std::string::npos) << error;

    directory.Write("mariadb/SHA256SUMS", sum(program) + "  bin/missing.exe\n");
    EXPECT_FALSE(HostPlay::VerifyDatabase(folder, error));
    EXPECT_NE(error.find("missing"), std::string::npos) << error;

    directory.Write("mariadb/SHA256SUMS", sum(program) + "  ../outside.exe\n");
    EXPECT_FALSE(HostPlay::VerifyDatabase(folder, error));
    EXPECT_NE(error.find("outside"), std::string::npos) << error;

    directory.Write("mariadb/SHA256SUMS", "");
    EXPECT_FALSE(HostPlay::VerifyDatabase(folder, error));
    EXPECT_FALSE(HostPlay::VerifyDatabase(directory.Path() / "absent", error));
}
