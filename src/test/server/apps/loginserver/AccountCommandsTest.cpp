/*
 * Project Ambrose by Imjustchico
 * Tests the login server's address and machine ban console commands, including their argument and hexadecimal MachineID validation without a database.
 */

#include "AccountCommands.h"
#include "ConsoleCommandTable.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::vector<std::string> RunLine(ConsoleCommandTable const& commands, std::string_view line, ConsoleCommandTable::Result& result)
    {
        std::vector<std::string> replies;
        result = commands.Execute(line, [&replies](std::string_view text) { replies.emplace_back(text); });
        return replies;
    }
}

TEST(LoginAccountCommandsTest, RegistersAndUnregistersNetworkBanCommands)
{
    ConsoleCommandTable commands;
    AccountCommands::Register(commands);

    std::vector<std::string> const all = commands.DescribeCommands();
    for (std::string const& expected : { "ban ip", "ban machine", "unban ip", "unban machine" })
        EXPECT_NE(std::find_if(all.begin(), all.end(), [&expected](std::string const& line) { return line.starts_with(expected); }), all.end()) << expected;

    AccountCommands::Unregister(commands);
    std::vector<std::string> const remaining = commands.DescribeCommands();
    for (std::string const& removed : { "ban ip", "ban machine", "unban ip", "unban machine" })
        EXPECT_EQ(std::find_if(remaining.begin(), remaining.end(), [&removed](std::string const& line) { return line.starts_with(removed); }), remaining.end()) << removed;
}

TEST(LoginAccountCommandsTest, RejectsMissingArgumentsAndMalformedMachineIds)
{
    ConsoleCommandTable commands;
    AccountCommands::Register(commands);
    ConsoleCommandTable::Result result;

    std::vector<std::string> replies = RunLine(commands, "ban ip 192.0.2.1 1h", result);
    ASSERT_FALSE(replies.empty());
    EXPECT_EQ(replies.back(), "Usage: ban ip <address> <duration|perm> <reason>");
    EXPECT_EQ(result, ConsoleCommandTable::Result::Usage);
    replies = RunLine(commands, "ban machine not-hex 1h reason", result);
    ASSERT_FALSE(replies.empty());
    EXPECT_EQ(replies.back(), "Usage: ban machine <machine-hex> <duration|perm> <reason>");
    EXPECT_EQ(result, ConsoleCommandTable::Result::Usage);
    replies = RunLine(commands, "unban machine not-hex", result);
    ASSERT_FALSE(replies.empty());
    EXPECT_EQ(replies.back(), "Usage: unban machine <machine-hex>");
    EXPECT_EQ(result, ConsoleCommandTable::Result::Usage);
    AccountCommands::Unregister(commands);
}

TEST(LoginAccountCommandsTest, RejectsMalformedAddressesBeforeReachingTheDatabase)
{
    ConsoleCommandTable commands;
    AccountCommands::Register(commands);
    ConsoleCommandTable::Result result;

    std::vector<std::string> const replies = RunLine(commands, "ban ip not-an-address 30m test-reason", result);

    ASSERT_EQ(result, ConsoleCommandTable::Result::Ran);
    ASSERT_EQ(replies.size(), 1u);
    EXPECT_EQ(replies[0], "Address not banned: that is not an IPv4 or IPv6 address");
    AccountCommands::Unregister(commands);
}
