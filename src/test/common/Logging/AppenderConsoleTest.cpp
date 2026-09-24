/*
 * Project Ambrose by Imjustchico
 * Tests the level word carrying the only level color, the fixed terminal columns, the category column's shortening, plain redirected output, color modes, NO_COLOR and prompt hooks.
 */

#include "AppenderConsole.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    class ColorEnvironment
    {
    public:
        ColorEnvironment()
        {
            _noColor = Ambrose::GetEnv("NO_COLOR");
            _force = Ambrose::GetEnv("CLICOLOR_FORCE");
            Ambrose::UnsetEnv("NO_COLOR");
            Ambrose::UnsetEnv("CLICOLOR_FORCE");
        }

        ~ColorEnvironment()
        {
            Restore("NO_COLOR", _noColor);
            Restore("CLICOLOR_FORCE", _force);
        }

    private:
        static void Restore(std::string const& name, std::optional<std::string> const& value)
        {
            if (value)
                Ambrose::SetEnv(name, *value);
            else
                Ambrose::UnsetEnv(name);
        }

        std::optional<std::string> _noColor;
        std::optional<std::string> _force;
    };

    std::size_t CountEscapes(std::string const& text)
    {
        return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\x1b'));
    }

    std::string StripEscapes(std::string const& text)
    {
        std::string out;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '' && i + 1 < text.size() && text[i + 1] == '[')
            {
                i += 2;
                while (i < text.size() && text[i] != 'm')
                    ++i;
                continue;
            }
            out.push_back(text[i]);
        }
        return out;
    }

    constexpr char NewLine = 0x0A;

    std::vector<std::string> SplitLines(std::string const& text)
    {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t const end = text.find(NewLine, start);
            if (end == std::string::npos)
                break;
            lines.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        return lines;
    }

    std::string const ConsoleBody = "Appender.Console = 1,1,2\nLogger.root = 1,Console\n";
}

TEST(AppenderConsoleTest, TerminalGetsAnsiColorsPerLevel)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Error, "server", "broken");
    AMBROSE_LOG(log, LogLevel::Warn, "server", "careful");
    AMBROSE_LOG(log, LogLevel::Fatal, "server", "gone");
    AMBROSE_LOG(log, LogLevel::Info, "server", "fine");
    EXPECT_EQ(harness.Device().Output(),
        "\x1b[91mERROR \x1b[0mbroken\n"
        "\x1b[33mWARN  \x1b[0mcareful\n"
        "\x1b[31mFATAL \x1b[0mgone\n"
        "\x1b[96mINFO  \x1b[0mfine\n");
}

TEST(AppenderConsoleTest, TimestampsAndCategoriesTakeThePrefixColor)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,7\nLogger.root = 1,Console\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "broken");
    std::string const output = harness.Device().Output();
    EXPECT_EQ(output.substr(0, 5), "\x1b[97m") << output;
    EXPECT_NE(output.find("\x1b[0m\x1b[91mERROR "), std::string::npos) << output;
    EXPECT_NE(output.find("\x1b[90m[server"), std::string::npos) << output;
    EXPECT_NE(StripEscapes(output).find("[server            ] broken"), std::string::npos) << output;
}

TEST(AppenderConsoleTest, DebugAndTraceCarryNoStateColor)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "server", "detail");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Trace, "server", "step");
    EXPECT_EQ(harness.Device().Output(), "\x1b[90mDEBUG detail\x1b[0m\n\x1b[90mTRACE step\x1b[0m\n");
}

TEST(AppenderConsoleTest, MultiLineMessagesColorEveryLine)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "one\ntwo");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR \x1b[0mone\n\x1b[91mERROR \x1b[0mtwo\n");
}

TEST(AppenderConsoleTest, RedirectedGetsNoEscapeSequences)
{
    ColorEnvironment environment;
    LogTestHarness harness(false, false);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "to a file");
    EXPECT_EQ(harness.Device().Output(), "ERROR to a file\n");
    EXPECT_FALSE(harness.Writer().UsesColor());
}

TEST(AppenderConsoleTest, NoColorEnvironmentDisablesAuto)
{
    ColorEnvironment environment;
    ASSERT_TRUE(Ambrose::SetEnv("NO_COLOR", "1"));
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "plain");
    EXPECT_EQ(CountEscapes(harness.Device().Output()), 0u);
}

TEST(AppenderConsoleTest, ClicolorForceUpgradesAuto)
{
    ColorEnvironment environment;
    ASSERT_TRUE(Ambrose::SetEnv("CLICOLOR_FORCE", "1"));
    LogTestHarness harness(false, false);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "forced");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR forced\x1b[0m\n");
}

TEST(AppenderConsoleTest, ColorsAlwaysForcesWhenRedirected)
{
    ColorEnvironment environment;
    LogTestHarness harness(false, false);
    harness.ApplyOrFail("Console.Colors = 2\n" + ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "always");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR always\x1b[0m\n");
    harness.ApplyOrFail("Console.Colors = 0\n" + ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "never");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR always\x1b[0m\nERROR never\n");
}

TEST(AppenderConsoleTest, LegacyFallbackUsesAttributes)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, false);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "legacy");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Warn, "server", "legacy warn");
    EXPECT_EQ(harness.Device().Output(), "ERROR legacy\nWARN  legacy warn\n");
    EXPECT_EQ(harness.Device().LegacyColors(), (std::vector<ConsoleColor>{ ConsoleColor::LightRed, ConsoleColor::Brown }));
    EXPECT_EQ(harness.Device().ResetCount(), 2u);
}

TEST(AppenderConsoleTest, LineHooksWrapEveryWrite)
{
    ColorEnvironment environment;
    LogTestHarness harness(false, false);
    harness.ApplyOrFail(ConsoleBody);
    harness.Writer().SetLineHooks([](ConsoleDevice& device) { device.Write("<erase prompt>"); }, [](ConsoleDevice& device) { device.Write("<redraw prompt>"); });
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "first");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "second");
    EXPECT_EQ(harness.Device().Output(), "<erase prompt>INFO  first\n<redraw prompt><erase prompt>INFO  second\n<redraw prompt>");
    harness.Writer().WithLock([](ConsoleDevice& device) { device.Write("typed"); });
    EXPECT_EQ(harness.Device().Output().substr(harness.Device().Output().size() - 5), "typed");
}

TEST(AppenderConsoleTest, CustomColorStringApplies)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,2,\"2 2 2 12 2 15\"\nLogger.root = 1,Console\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "light magenta");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Trace, "server", "default");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "server", "green");
    EXPECT_EQ(harness.Device().Output(), "\x1b[95mINFO  \x1b[0mlight magenta\nTRACE default\n\x1b[32mDEBUG green\x1b[0m\n");
    std::shared_ptr<AppenderConsole> const console = std::dynamic_pointer_cast<AppenderConsole>(harness.GetLog().GetAppender("Console"));
    ASSERT_NE(console, nullptr);
    EXPECT_EQ(console->GetColors()[static_cast<std::size_t>(LogLevel::Info)], ConsoleColor::LightMagenta);
    EXPECT_EQ(console->GetColors()[static_cast<std::size_t>(LogLevel::Warn)], ConsoleColor::Green);
}

TEST(AppenderConsoleTest, InvalidColorStringIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,3,0,\"1 2 3\"\nLogger.root = 3,Console\n", result);
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_NE(result.Errors[0].Message.find("six codes"), std::string::npos);
    LogConfigResult range;
    LogTestConfig::Settings("Appender.Console = 1,3,0,\"1 2 3 4 5 16\"\nLogger.root = 3,Console\n", range);
    EXPECT_EQ(range.Errors.size(), 1u);
}

TEST(AppenderConsoleTest, RestoreDisablesColorForLateLines)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "colored");
    harness.GetLog().Shutdown();
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "late");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR \x1b[0mcolored\nERROR late\n");
    EXPECT_EQ(harness.Device().RestoreCount(), 1u);
    harness.GetLog().Shutdown();
    EXPECT_EQ(harness.Device().RestoreCount(), 1u);
}

TEST(AppenderConsoleTest, EscapeInPlayerTextNeutralized)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Console.Colors = 0\n" + ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.chat", "{}", "\x1b[2J\x1b[31mhacked");
    EXPECT_EQ(harness.Device().Output(), "INFO  \\x1B[2J\\x1B[31mhacked\n");
    EXPECT_EQ(CountEscapes(harness.Device().Output()), 0u);
}


TEST(AppenderConsoleTest, EveryLevelLeavesTheBodyInTheBodyColor)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,2\nLogger.root = 1,Console\n");
    for (LogLevel const level : { LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal })
        AMBROSE_LOG(harness.GetLog(), level, "server", "the body");
    std::string const output = harness.Device().Output();
    for (std::string const& line : SplitLines(output))
    {
        std::size_t const body = line.find("the body");
        ASSERT_NE(body, std::string::npos) << line;
        std::string const before = line.substr(0, body);
        EXPECT_EQ(before.find("\x1b[0m") != std::string::npos, true) << line;
        EXPECT_EQ(line.find("the body\x1b["), std::string::npos) << line;
    }
}

TEST(AppenderConsoleTest, EveryMessageStartsAtTheSameColumn)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,7\nLogger.root = 1,Console\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "a", "one");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Warn, "server.login", "two");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server.database.pool", "three");
    std::vector<std::string> const lines = SplitLines(StripEscapes(harness.Device().Output()));
    ASSERT_EQ(lines.size(), 3u);
    std::size_t const first = lines[0].find("one");
    ASSERT_NE(first, std::string::npos) << lines[0];
    EXPECT_EQ(lines[1].find("two"), first) << lines[1];
    EXPECT_EQ(lines[2].find("three"), first) << lines[2];
    EXPECT_EQ(lines[0].size() - 3, first);
}

TEST(AppenderConsoleTest, ALongCategoryIsShortenedInTheMiddleAndTheRecordKeepsItWhole)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,7\nAppender.Capture = 200,1,0\nLogger.root = 1,Console,Capture\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.database.connection.pool", "held");
    std::string const shown = StripEscapes(harness.Device().Output());
    EXPECT_NE(shown.find(".."), std::string::npos) << shown;
    EXPECT_EQ(shown.find("server.database.connection.pool"), std::string::npos) << shown;
    std::vector<LogMessage> const captured = harness.Store().Messages("Capture");
    ASSERT_EQ(captured.size(), 1u);
    EXPECT_EQ(captured[0].Category, "server.database.connection.pool");
}

TEST(AppenderConsoleTest, RedirectedOutputKeepsTheFullDateAndTheUnpaddedCategory)
{
    ColorEnvironment environment;
    LogTestHarness harness(false, false);
    harness.ApplyOrFail("Appender.Console = 1,1,7\nLogger.root = 1,Console\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server.login", "to a file");
    std::string const output = harness.Device().Output();
    EXPECT_EQ(CountEscapes(output), 0u) << output;
    EXPECT_NE(output.find(" ERROR [server.login] to a file\n"), std::string::npos) << output;
    EXPECT_EQ(output.find('-'), 4u) << output;
    EXPECT_EQ(output.find(':'), 13u) << output;
}

TEST(AppenderConsoleTest, NamedColorsSetTheRolesAndTheLevels)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail("Appender.Console = 1,1,2,\"body=10 info=13\"\nLogger.root = 1,Console\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "named");
    std::string const output = harness.Device().Output();
    EXPECT_NE(output.find("INFO  \x1b[0m\x1b["), std::string::npos) << output;
    EXPECT_NE(output.find("named\x1b[0m"), std::string::npos) << output;
}

TEST(AppenderConsoleTest, AnUnknownColorNameIsAnError)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    LogConfigResult const result = harness.Apply("Appender.Console = 1,1,2,\"shoulder=10\"\nLogger.root = 1,Console\n");
    EXPECT_FALSE(result.Errors.empty());
}
