/*
 * Project Ambrose by Imjustchico
 * Tests environment variables round-tripping names and values with non-ASCII UTF-8 and disappearing when unset, text that is not UTF-8 refused, and the arguments read as UTF-8 from the wide command line on Windows or passed through elsewhere.
 */

#include "Environment.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(EnvironmentTest, VariablesRoundTripUtf8NamesAndValues)
{
    std::string const name = "AMBROSE_ENVIRONMENT_TEST_\xC3\xA9";
    std::string const value = "C:/Users/Jos\xC3\xA9/Wizard101 \xE2\x9C\x93";
    ASSERT_TRUE(Ambrose::SetEnv(name, value));
    EXPECT_EQ(Ambrose::GetEnv(name), value);
    ASSERT_TRUE(Ambrose::SetEnv(name, "plain"));
    EXPECT_EQ(Ambrose::GetEnv(name), "plain");
    ASSERT_TRUE(Ambrose::UnsetEnv(name));
    EXPECT_FALSE(Ambrose::GetEnv(name));
#ifdef _WIN32
    EXPECT_FALSE(Ambrose::SetEnv(name, "bad \xFF byte"));
#endif
}

TEST(EnvironmentTest, ArgumentsAreUtf8)
{
    std::string first = "tool";
    std::string second = "caf\xC3\xA9";
    char* argv[] = { first.data(), second.data() };
    std::vector<std::string> const arguments = Ambrose::GetArguments(2, argv);
    ASSERT_FALSE(arguments.empty());
#ifdef _WIN32
    EXPECT_NE(arguments.front().find("unit_tests"), std::string::npos) << arguments.front();
#else
    EXPECT_EQ(arguments, (std::vector<std::string>{ "tool", "caf\xC3\xA9" }));
#endif
}
