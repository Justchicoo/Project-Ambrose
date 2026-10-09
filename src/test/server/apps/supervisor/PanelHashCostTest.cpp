/*
 * Project Ambrose by Imjustchico
 * Lowers the panel's Argon2id cost for the whole test program before its first test, because a Debug build hashes at the real cost for seconds a time and the panel tests sign in thousands of times, which made them most of a Windows test run; checks the real cost is still the one the panel ships with, that a hash keeps the cost it was made at in its own text, and that one made at the lower cost still opens once the cost is raised again.
 */

#include "PanelUsers.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    PanelHashCost const TestCost{ 1, 4 * 1024, 1 };

    class PanelHashCostEnvironment : public testing::Environment
    {
    public:
        void SetUp() override
        {
            PanelUsers::SetHashCost(TestCost);
        }
    };

    testing::Environment* const Registered = testing::AddGlobalTestEnvironment(new PanelHashCostEnvironment);

    class PanelHashCostTest : public testing::Test
    {
    protected:
        void TearDown() override
        {
            PanelUsers::SetHashCost(TestCost);
        }
    };
}

TEST_F(PanelHashCostTest, TheShippedCostIsSixtyFourMebibytesOverThreePasses)
{
    PanelHashCost const shipped;
    EXPECT_EQ(shipped.Lanes, 1u);
    EXPECT_EQ(shipped.MemoryKiB, 64u * 1024u);
    EXPECT_EQ(shipped.Passes, 3u);

    PanelUsers::SetHashCost(shipped);
    std::string hash;
    std::string error;
    ASSERT_TRUE(PanelUsers::HashPassword("a good long password", hash, error)) << error;
    EXPECT_NE(hash.find("$m=65536,t=3,p=1$"), std::string::npos) << hash;
    EXPECT_TRUE(PanelUsers::PasswordMatches(hash, "a good long password"));
}

TEST_F(PanelHashCostTest, AHashMadeAtTheTestCostStillOpensAtTheShippedCost)
{
    ASSERT_NE(Registered, nullptr);
    EXPECT_EQ(PanelUsers::GetHashCost().MemoryKiB, TestCost.MemoryKiB);
    std::string hash;
    std::string error;
    ASSERT_TRUE(PanelUsers::HashPassword("a good long password", hash, error)) << error;
    EXPECT_NE(hash.find("$m=4096,t=1,p=1$"), std::string::npos) << hash;

    PanelUsers::SetHashCost(PanelHashCost{});
    EXPECT_TRUE(PanelUsers::PasswordMatches(hash, "a good long password"));
    EXPECT_FALSE(PanelUsers::PasswordMatches(hash, "a good long passwore"));
}
