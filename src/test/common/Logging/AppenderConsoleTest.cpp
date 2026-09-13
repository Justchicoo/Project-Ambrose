/*
 * Project Ambrose by Imjustchico
 * Tests colors on terminals, plain redirected output, color modes, NO_COLOR and prompt hooks.
 */

#include "AppenderConsole.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <algorithm>

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
        "\x1b[91mERROR broken\x1b[0m\n"
        "\x1b[33mWARN  careful\x1b[0m\n"
        "\x1b[31mFATAL gone\x1b[0m\n"
        "\x1b[36mINFO  fine\x1b[0m\n");
}

TEST(AppenderConsoleTest, MultiLineMessagesColorEveryLine)
{
    ColorEnvironment environment;
    LogTestHarness harness(true, true);
    harness.ApplyOrFail(ConsoleBody);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "one\ntwo");
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR one\x1b[0m\n\x1b[91mERROR two\x1b[0m\n");
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
    EXPECT_EQ(harness.Device().Output(), "\x1b[95mINFO  light magenta\x1b[0m\nTRACE default\n\x1b[32mDEBUG green\x1b[0m\n");
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
    EXPECT_EQ(harness.Device().Output(), "\x1b[91mERROR colored\x1b[0m\nERROR late\n");
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
