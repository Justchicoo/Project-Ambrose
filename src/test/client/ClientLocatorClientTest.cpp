/*
 * Project Ambrose by Imjustchico
 * Runs client discovery on the real machine when AMBROSE_CLIENT_DIR names the user's own install: that install inspects as the pinned revision and is found through the environment, every install found is printed with where it was found, and when AMBROSE_TYPE_DUMP_PATH names the type dump it reads as a dump and is found first.
 */

#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "Environment.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

TEST(ClientLocatorClientTest, TheNamedInstallAndDumpAreFoundOnThisMachine)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!client || client->empty())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set";
    LocalClientSystem const system;
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, ConfigMgr::PathFromUtf8(*client));
    ASSERT_TRUE(install);
    EXPECT_TRUE(install->IsPinned()) << install->Describe();

    std::vector<ClientCandidate> const installs = ClientLocator::FindInstalls(system);
    for (ClientCandidate const& candidate : installs)
        std::cout << "[ INSTALL  ] " << candidate.Install.Describe() << ", found through " << candidate.Source << std::endl;
    auto const named = std::find_if(installs.begin(), installs.end(), [](ClientCandidate const& candidate) { return candidate.Source == "AMBROSE_CLIENT_DIR"; });
    ASSERT_NE(named, installs.end());
    EXPECT_TRUE(named->Install.IsPinned());
    EXPECT_TRUE(installs.front().Install.IsPinned());

    std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!dump || dump->empty())
        return;
    EXPECT_TRUE(ClientLocator::LooksLikeTypeDump(system, ConfigMgr::PathFromUtf8(*dump)));
    std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, installs);
    ASSERT_FALSE(dumps.empty());
    EXPECT_EQ(dumps.front().Source, "through AMBROSE_TYPE_DUMP_PATH");
    EXPECT_EQ(ClientLocator::PathText(dumps.front().Path), ClientLocator::PathText(ConfigMgr::PathFromUtf8(*dump).lexically_normal()));
}
