/*
 * Project Ambrose by Imjustchico
 * Tests console command matching: multi-word and case-insensitive names, the longest match, usage replies, unknown commands, group listings, quoted arguments, hidden sensitive arguments, and handlers that read the table.
 */

#include "ConsoleCommandTable.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    struct Transcript
    {
        std::vector<std::string> Lines;

        ConsoleCommandTable::Reply Reply()
        {
            return [this](std::string_view text) { Lines.emplace_back(text); };
        }
    };
}

TEST(ConsoleCommandTableTest, SplitsWordsAndQuotedArguments)
{
    EXPECT_EQ(ConsoleCommandTable::Split("  account  create\tbob  "), (std::vector<std::string>{ "account", "create", "bob" }));
    std::string const quoted = "ban bob \"two words\" \"say \\\"hi\\\"\" \"back\\\\slash\" \"\"";
    EXPECT_EQ(ConsoleCommandTable::Split(quoted), (std::vector<std::string>{ "ban", "bob", "two words", "say \"hi\"", "back\\slash", "" }));
    std::string const unterminated = "a \"unterminated rest";
    EXPECT_EQ(ConsoleCommandTable::Split(unterminated), (std::vector<std::string>{ "a", "unterminated rest" }));
    EXPECT_TRUE(ConsoleCommandTable::Split(" \t\r\n").empty());
}

TEST(ConsoleCommandTableTest, RunsTheLongestCaseInsensitiveMatch)
{
    ConsoleCommandTable table;
    std::vector<std::string> seen;
    std::string ran;
    ASSERT_TRUE(table.Register({ "account set", "<x>", "set things", false, [&](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const&) { ran = "set"; seen = arguments; return true; } }));
    ASSERT_TRUE(table.Register({ "Account  Set GMLevel", "<user> <level>", "set a level", false, [&](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const&) { ran = "gmlevel"; seen = arguments; return true; } }));
    EXPECT_FALSE(table.Register({ "account set gmlevel", "", "", false, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } }));
    EXPECT_FALSE(table.Register({ "   ", "", "", false, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } }));
    EXPECT_FALSE(table.Register({ "nohandler", "", "", false, nullptr }));

    Transcript transcript;
    EXPECT_EQ(table.Execute("ACCOUNT set gmlevel Bob 3", transcript.Reply()), ConsoleCommandTable::Result::Ran);
    EXPECT_EQ(ran, "gmlevel");
    EXPECT_EQ(seen, (std::vector<std::string>{ "Bob", "3" }));
    EXPECT_EQ(table.Execute("account set other", transcript.Reply()), ConsoleCommandTable::Result::Ran);
    EXPECT_EQ(ran, "set");
    EXPECT_EQ(seen, (std::vector<std::string>{ "other" }));
    EXPECT_EQ(table.Execute("   ", transcript.Reply()), ConsoleCommandTable::Result::Empty);
    EXPECT_TRUE(transcript.Lines.empty());

    EXPECT_TRUE(table.Unregister("ACCOUNT SET GMLEVEL"));
    EXPECT_FALSE(table.Unregister("account set gmlevel"));
    EXPECT_EQ(table.Execute("account set gmlevel bob 3", transcript.Reply()), ConsoleCommandTable::Result::Ran);
    EXPECT_EQ(ran, "set");
}

TEST(ConsoleCommandTableTest, RepliesUsageUnknownAndGroups)
{
    ConsoleCommandTable table;
    table.Register({ "account create", "<username> <password>", "create an account", true, [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const&) { return arguments.size() == 2; } });
    table.Register({ "account info", "<username>", "describe an account", false, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } });

    Transcript usage;
    EXPECT_EQ(table.Execute("account create onlyname", usage.Reply()), ConsoleCommandTable::Result::Usage);
    EXPECT_EQ(usage.Lines, (std::vector<std::string>{ "Usage: account create <username> <password>" }));

    Transcript unknown;
    EXPECT_EQ(table.Execute("acount create bob secret", unknown.Reply()), ConsoleCommandTable::Result::Unknown);
    EXPECT_EQ(unknown.Lines, (std::vector<std::string>{ "Unknown command 'acount'. Type 'help' to list commands." }));

    Transcript group;
    EXPECT_EQ(table.Execute("account bogus", group.Reply()), ConsoleCommandTable::Result::Usage);
    EXPECT_EQ(group.Lines, (std::vector<std::string>{ "Commands starting with 'account':", "  account create <username> <password> - create an account", "  account info <username> - describe an account" }));

    EXPECT_EQ(table.DescribeCommands("ACCOUNT info"), (std::vector<std::string>{ "account info <username> - describe an account" }));
    EXPECT_TRUE(table.DescribeCommands("nothing").empty());
}

TEST(ConsoleCommandTableTest, LogDescriptionsHideSensitiveArguments)
{
    ConsoleCommandTable table;
    table.Register({ "account create", "<username> <password>", "", true, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } });
    table.Register({ "account info", "<username>", "", false, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } });
    EXPECT_EQ(table.DescribeForLog("account create bob hunter2"), "account create (arguments hidden)");
    EXPECT_EQ(table.DescribeForLog("Account Create"), "account create");
    EXPECT_EQ(table.DescribeForLog("account info  bob"), "account info bob");
    EXPECT_EQ(table.DescribeForLog("acount create bob hunter2"), "unknown command 'acount'");
    EXPECT_EQ(table.DescribeForLog("  "), "");
}

TEST(ConsoleCommandTableTest, HandlersMayReadAndChangeTheTable)
{
    ConsoleCommandTable table;
    table.Register({ "help", "", "", false, [&table](std::vector<std::string> const&, ConsoleCommandTable::Reply const& reply)
    {
        for (std::string const& line : table.DescribeCommands())
            reply(line);
        table.Register({ "later", "", "added by help", false, [](std::vector<std::string> const&, ConsoleCommandTable::Reply const&) { return true; } });
        return true;
    } });
    Transcript transcript;
    EXPECT_EQ(table.Execute("help", transcript.Reply()), ConsoleCommandTable::Result::Ran);
    EXPECT_EQ(transcript.Lines, (std::vector<std::string>{ "help" }));
    EXPECT_EQ(table.Execute("later", transcript.Reply()), ConsoleCommandTable::Result::Ran);
}
