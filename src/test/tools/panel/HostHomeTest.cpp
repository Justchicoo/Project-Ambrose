/*
 * Project Ambrose by Imjustchico
 * Checks the server home the panel program hosts from: a supervisor is found beside the program or where Host.Supervisor names it and nowhere else, so with neither the program runs connect-only; preparing the home twice refreshes every .conf.dist but never overwrites an edited .conf; and the panel's port is 12080 when free, moves to the next free one when taken, and is remembered in the program's own conf.d file.
 */

#include "HostHome.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace
{
    std::string Read(std::filesystem::path const& file)
    {
        std::ifstream held(file, std::ios::binary);
        std::ostringstream text;
        text << held.rdbuf();
        return text.str();
    }

    std::filesystem::path Supervisor(LogTestDirectory const& directory)
    {
#ifdef _WIN32
        return directory.Write("bin/supervisor.exe", "program");
#else
        return directory.Write("bin/supervisor", "program");
#endif
    }
}

TEST(HostHomeTest, ASupervisorIsFoundBesideTheProgramOrWhereItIsNamedAndOtherwiseItRunsConnectOnly)
{
    LogTestDirectory directory;
    EXPECT_FALSE(HostHome::FindSupervisor(directory.Path() / "bin", {}).has_value());
    EXPECT_NE(std::string(HostHome::ConnectOnly).find("server component"), std::string::npos);
    std::filesystem::path const supervisor = Supervisor(directory);
    EXPECT_EQ(HostHome::FindSupervisor(directory.Path() / "bin", {}), supervisor);
    EXPECT_FALSE(HostHome::FindSupervisor(directory.Path() / "bin", directory.Path() / "elsewhere" / "supervisor").has_value())
        << "a named supervisor that is not there is not replaced by the one beside the program";
    EXPECT_EQ(HostHome::FindSupervisor(directory.Path() / "other", supervisor), supervisor);
}

TEST(HostHomeTest, PreparingTwiceNeverOverwritesAnEditedConfAndRefreshesEveryDist)
{
    LogTestDirectory directory;
    std::filesystem::path const supervisor = Supervisor(directory);
    directory.Write("bin/supervisor.conf.dist", "Admin.Port = 12020\n");
    directory.Write("bin/loginserver.conf.dist", "Port = 12000\n");
    std::string error;
    std::optional<HostPrepared> const first = HostHome::Prepare(directory.Path() / "data", supervisor, [](uint16) { return true; }, error);
    ASSERT_TRUE(first.has_value()) << error;
    EXPECT_EQ(std::set<std::string>(first->Made.begin(), first->Made.end()), (std::set<std::string>{ "supervisor.conf", "loginserver.conf" }));
    EXPECT_EQ(first->Config, first->Home / "supervisor.conf");

    std::ofstream(first->Home / "loginserver.conf", std::ios::binary | std::ios::trunc) << "Port = 12345\n";
    directory.Write("bin/loginserver.conf.dist", "Port = 12000\nNew.Option = 1\n");
    std::optional<HostPrepared> const second = HostHome::Prepare(directory.Path() / "data", supervisor, [](uint16) { return true; }, error);
    ASSERT_TRUE(second.has_value()) << error;
    EXPECT_TRUE(second->Made.empty());
    EXPECT_EQ(Read(second->Home / "loginserver.conf"), "Port = 12345\n") << "an edited .conf is never overwritten";
    EXPECT_EQ(Read(second->Home / "loginserver.conf.dist"), "Port = 12000\nNew.Option = 1\n") << "each start copies the running build's .conf.dist in";
}

TEST(HostHomeTest, ATakenPanelPortMovesToTheNextFreeOneAndIsRemembered)
{
    LogTestDirectory directory;
    std::filesystem::path const supervisor = Supervisor(directory);
    std::string error;
    std::set<uint16> taken{ 12080, 12081 };
    std::optional<HostPrepared> const moved = HostHome::Prepare(directory.Path() / "data", supervisor, [&taken](uint16 port) { return !taken.contains(port); }, error);
    ASSERT_TRUE(moved.has_value()) << error;
    EXPECT_EQ(moved->PanelPort, 12082);
    std::filesystem::path const own = moved->Home / "conf.d" / HostHome::OwnFile;
    EXPECT_EQ(HostHome::RememberedPort(own), 12082);
    std::string const written = Read(own);
    EXPECT_NE(written.find("Panel.Enable = 1"), std::string::npos);
    EXPECT_NE(written.find("Panel.BindIP = 127.0.0.1"), std::string::npos);

    taken.clear();
    std::optional<HostPrepared> const kept = HostHome::Prepare(directory.Path() / "data", supervisor, [&taken](uint16 port) { return !taken.contains(port); }, error);
    ASSERT_TRUE(kept.has_value()) << error;
    EXPECT_EQ(kept->PanelPort, 12082) << "a remembered port that is free stays, even once 12080 is free again";
}
