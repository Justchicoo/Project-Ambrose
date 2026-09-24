/*
 * Project Ambrose by Imjustchico
 * Tests the appender and logger grammar, defaults, and every error and warning with file and line.
 */

#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "LogConfig.h"
#include "LogTestConfig.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <map>

namespace
{
    bool AnyIssueContains(std::vector<ConfigIssue> const& issues, std::string_view text)
    {
        for (ConfigIssue const& issue : issues)
            if (issue.ToString().find(text) != std::string::npos)
                return true;
        return false;
    }

    std::string const Root = "Logger.root = 3,Console\nAppender.Console = 1,3,0\n";
}

TEST(LogConfigTest, ParsesConsoleLineWithQuotedColors)
{
    LogConfigResult result;
    LogSettings const settings = LogTestConfig::Settings("Appender.Console = 1,3,3,\"1 9 3 6 5 8\"\nLogger.root = 3,Console\n", result);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    ASSERT_EQ(settings.Appenders.size(), 1u);
    AppenderDefinition const& console = settings.Appenders[0];
    EXPECT_EQ(console.Name, "Console");
    EXPECT_EQ(console.Type, AppenderType::Console);
    EXPECT_EQ(console.Level, LogLevel::Info);
    EXPECT_EQ(console.Flags, AppenderFlags::PrefixTimestamp | AppenderFlags::PrefixLevel);
    ASSERT_EQ(console.Fields.size(), 1u);
    EXPECT_EQ(console.Fields[0], "1 9 3 6 5 8");
    EXPECT_EQ(console.Line, 3u);
    std::optional<AppenderConsole::ColorTable> const colors = AppenderConsole::ParseColors(console.Fields[0]);
    ASSERT_TRUE(colors.has_value());
    EXPECT_EQ((*colors)[static_cast<std::size_t>(LogLevel::Fatal)], ConsoleColor::Red);
    EXPECT_EQ((*colors)[static_cast<std::size_t>(LogLevel::Trace)], ConsoleColor::Yellow);
    ASSERT_EQ(settings.Loggers.size(), 1u);
    EXPECT_EQ(settings.Loggers[0].Appenders, std::vector<std::string>{ "Console" });
}

TEST(LogConfigTest, FileFieldsAndDefaults)
{
    LogConfigResult result;
    LogSettings const settings = LogTestConfig::Settings("LogsDir = /var/log/ambrose\nAppender.Server = 2,2,7,Server.log\nAppender.World = File,debug,0x07,World.log,w,64M,10,1000\nLogger.root = info,Server World\n", result);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    EXPECT_EQ(settings.LogsDir, std::filesystem::path("/var/log/ambrose"));
    EXPECT_FALSE(settings.AsyncEnable);
    EXPECT_EQ(settings.AsyncQueueSize, 65536u);
    EXPECT_EQ(settings.PendingBuffer, 1000u);

    LogConfigResult fileResult;
    std::optional<AppenderFileSettings> const server = AppenderFile::ParseSettings(*settings.FindAppender("Server"), settings.LogsDir, fileResult);
    ASSERT_TRUE(server.has_value());
    EXPECT_EQ(server->Options.Mode, LogFileMode::Append);
    EXPECT_EQ(server->Options.MaxFileSize, 0u);
    EXPECT_EQ(server->Options.MaxBackups, 0u);
    EXPECT_EQ(server->Options.FlushInterval.count(), 0);
    EXPECT_EQ(server->Path.filename(), "Server.log");

    std::optional<AppenderFileSettings> const world = AppenderFile::ParseSettings(*settings.FindAppender("World"), settings.LogsDir, fileResult);
    ASSERT_TRUE(world.has_value());
    EXPECT_EQ(world->Options.Mode, LogFileMode::Truncate);
    EXPECT_EQ(world->Options.MaxFileSize, 64u * 1024 * 1024);
    EXPECT_EQ(world->Options.MaxBackups, 10u);
    EXPECT_EQ(world->Options.FlushInterval.count(), 1000);
    EXPECT_TRUE(fileResult.Succeeded());
}

TEST(LogConfigTest, QuotedFileNameMayContainCommas)
{
    LogConfigResult result;
    LogSettings const settings = LogTestConfig::Settings("Appender.Odd = 2,2,0,\"odd,name.log\",a\nLogger.root = 3,Odd\n", result);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    ASSERT_EQ(settings.Appenders[0].Fields.size(), 2u);
    EXPECT_EQ(settings.Appenders[0].Fields[0], "odd,name.log");
}

TEST(LogConfigTest, AppenderNameWithDotIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Server.Main = 2,2,0,Server.log\n" + Root, result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "appender names cannot contain '.'")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, BadLevelNamesFileLineAndRange)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,9,0\nLogger.root = 3,Console\n", result);
    ASSERT_EQ(result.Errors.size(), 1u) << LogTestConfig::Describe(result);
    std::string const message = result.Errors[0].ToString();
    EXPECT_EQ(message.rfind("app.conf:3: Appender.Console: level '9' is out of range", 0), 0u) << message;
    EXPECT_NE(message.find("0-6"), std::string::npos);

    LogConfigResult loggerResult;
    LogTestConfig::Settings("Appender.Console = 1,3,0\nLogger.root = loud,Console\n", loggerResult);
    ASSERT_EQ(loggerResult.Errors.size(), 1u);
    EXPECT_EQ(loggerResult.Errors[0].ToString().rfind("app.conf:4: Logger.root: level 'loud'", 0), 0u) << loggerResult.Errors[0].ToString();
}

TEST(LogConfigTest, UnknownFlagBitsAreAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,3,0x40\nLogger.root = 3,Console\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "flags '0x40' are not valid")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, TooFewAndTooManyFieldsAreErrors)
{
    LogConfigResult few;
    LogTestConfig::Settings("Appender.Console = 1,3\nLogger.root = 3,Console\n", few);
    EXPECT_TRUE(AnyIssueContains(few.Errors, "expected Type,Level,Flags"));

    LogConfigResult many;
    LogTestConfig::Settings("Appender.Console = 1,3,0,\"1 9 3 6 5 8\",extra\nAppender.Server = 2,2,0,a.log,a,0,0,0,surplus\nAppender.Stream = 3,2,0,10,more\nLogger.root = 3,Console Server Stream\n", many);
    EXPECT_TRUE(AnyIssueContains(many.Errors, "Appender.Console: too many fields")) << LogTestConfig::Describe(many);
    EXPECT_TRUE(AnyIssueContains(many.Errors, "Appender.Server: too many fields"));
    EXPECT_TRUE(AnyIssueContains(many.Errors, "Appender.Stream: too many fields"));
}

TEST(LogConfigTest, FileNameWithPercentSIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Server = 2,2,0,Server_%s.log\nLogger.root = 3,Server\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "cannot contain '%s'")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, InvalidFileFieldsAreErrors)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.A = 2,2,0,a.log,x\nAppender.B = 2,2,0,b.log,a,512\nAppender.C = 2,2,0,c.log,a,0,1001\nAppender.D = 2,2,0,d.log,a,0,0,60001\nAppender.E = 2,2,0,\nLogger.root = 3,A B C D E\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.A: mode 'x' is not valid")) << LogTestConfig::Describe(result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.B: MaxFileSize '512'"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.C: MaxBackups '1001'"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.D: FlushIntervalMs '60001'"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.E: a File appender needs a file name"));
}

TEST(LogConfigTest, SizeBelowOneKilobyteIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Server = 2,2,0,Server.log,a,1023\nLogger.root = 3,Server\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "MaxFileSize '1023'")) << LogTestConfig::Describe(result);
    LogConfigResult ok;
    LogTestConfig::Settings("Appender.Server = 2,2,0,Server.log,a,1K\nLogger.root = 3,Server\n", ok);
    EXPECT_TRUE(ok.Succeeded()) << LogTestConfig::Describe(ok);
}

TEST(LogConfigTest, TwoFileAppendersOnOnePathIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.A = 2,2,0,Server.log\nAppender.B = 2,3,0,./Server.log\nLogger.root = 3,A B\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Appender.B: writes to the same file as Appender.A")) << LogTestConfig::Describe(result);
#ifdef _WIN32
    LogConfigResult caseResult;
    LogTestConfig::Settings("Appender.A = 2,2,0,Server.log\nAppender.B = 2,3,0,SERVER.LOG\nLogger.root = 3,A B\n", caseResult);
    EXPECT_TRUE(AnyIssueContains(caseResult.Errors, "writes to the same file")) << LogTestConfig::Describe(caseResult);
#endif
}

TEST(LogConfigTest, SecondStreamAppenderIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.S1 = 3,2,0\nAppender.S2 = 3,2,0\nLogger.root = 3,S1 S2\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "only one Stream appender is allowed")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, MissingLoggerRootIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,3,0\nLogger.server = 3,Console\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Logger.root is required")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, UnknownAppenderReferenceListsDefinedAppenders)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,3,0\nAppender.Server = 2,2,0,Server.log\nLogger.root = 3,Console Server\nLogger.sql.sql = 4,Sever\n", result);
    ASSERT_EQ(result.Errors.size(), 1u) << LogTestConfig::Describe(result);
    EXPECT_EQ(result.Errors[0].ToString(), "app.conf:6: Logger.sql.sql: references appender 'Sever', which is not defined; defined appenders: Console, Server");
}

TEST(LogConfigTest, EmptyAndDuplicateAppenderListsWarn)
{
    LogConfigResult result;
    LogSettings const settings = LogTestConfig::Settings(Root + "Logger.quiet = 3,\nLogger.twice = 3,Console,Console\n", result);
    EXPECT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "Logger.quiet: no appenders are listed"));
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "Logger.twice: appender 'Console' is listed more than once"));
    EXPECT_EQ(settings.FindLogger("twice")->Appenders, std::vector<std::string>{ "Console" });
}

TEST(LogConfigTest, MissingAppenderListIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings(Root + "Logger.bare = 3\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Logger.bare: expected Level,Appender")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, UnusedAppenderWarns)
{
    LogConfigResult result;
    LogTestConfig::Settings(Root + "Appender.Spare = 2,2,0,Spare.log\n", result);
    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "Appender.Spare: no logger uses this appender")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, RootChildNameIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings(Root + "Logger.root.child = 3,Console\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "root has no children")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, FileFlagsOnConsoleWarn)
{
    LogConfigResult result;
    LogTestConfig::Settings("Appender.Console = 1,3,0x18\nLogger.root = 3,Console\n", result);
    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "apply only to File appenders")) << LogTestConfig::Describe(result);
    LogConfigResult backup;
    LogTestConfig::Settings("Appender.Server = 2,2,0x10,Server.log,a\nLogger.root = 3,Server\n", backup);
    EXPECT_TRUE(AnyIssueContains(backup.Warnings, "flag 0x10 backs up the file only with mode w")) << LogTestConfig::Describe(backup);
}

TEST(LogConfigTest, UnknownLogOptionWarns)
{
    LogConfigResult result;
    LogTestConfig::Settings(Root + "Log.Asynk.Enable = 1\n", result);
    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "Log.Asynk.Enable is not a logging option")) << LogTestConfig::Describe(result);
}

TEST(LogConfigTest, UnregisteredTypeIsInactiveNotError)
{
    LogConfigResult parsed;
    LogSettings settings = LogTestConfig::Settings(Root + "Appender.DB = 4,2,0\nLogger.server = 2,Console DB\n", parsed);
    ASSERT_TRUE(parsed.Succeeded()) << LogTestConfig::Describe(parsed);
    LogTestHarness harness;
    LogConfigResult const applied = harness.GetLog().Apply(std::move(settings));
    ASSERT_TRUE(applied.Succeeded()) << LogTestConfig::Describe(applied);
    EXPECT_EQ(applied.InactiveAppenders, std::vector<std::string>{ "DB" });
    EXPECT_EQ(harness.GetLog().GetPendingAppenderNames(), std::vector<std::string>{ "DB" });
}

TEST(LogConfigTest, InvalidAsyncOptionIsAnError)
{
    LogConfigResult result;
    LogTestConfig::Settings(Root + "Log.Async.Enable = maybe\nLog.Async.QueueSize = 100\nLog.Async.QueueFull = 2\nLog.PendingBuffer = 100001\nLog.Utc = 2\nConsole.Colors = 3\n", result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Log.Async.Enable: 'maybe' is not a boolean")) << LogTestConfig::Describe(result);
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Log.Async.QueueSize: '100' is out of range"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Log.Async.QueueFull: '2' is not valid"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Log.PendingBuffer: '100001' is out of range"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Log.Utc: '2' is not a boolean"));
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Console.Colors: '3' is not valid"));
}

TEST(LogConfigTest, ValidOptionsParse)
{
    LogSettings const settings = LogTestConfig::Settings(Root + "Log.Async.Enable = yes\nLog.Async.QueueSize = 2048\nLog.Async.QueueFull = 1\nLog.PendingBuffer = 0\nLog.Utc = TRUE\nConsole.Colors = 2\n");
    EXPECT_TRUE(settings.AsyncEnable);
    EXPECT_EQ(settings.AsyncQueueSize, 2048u);
    EXPECT_EQ(settings.AsyncQueueFull, LogOverflowPolicy::Drop);
    EXPECT_EQ(settings.PendingBuffer, 0u);
    EXPECT_TRUE(settings.Utc);
    EXPECT_EQ(settings.ConsoleColors, ConsoleColorMode::Always);
}

TEST(LogConfigTest, EnvironmentOverridesExistingAppenderLine)
{
    LogTestDirectory directory;
    std::map<std::string, std::string> environment{ { "AMBROSE_LOGGER_SQL_SQL", "4,Capture" }, { "AMBROSE_LOG_ASYNC_ENABLE", "1" } };
    std::unique_ptr<ConfigMgr> const config = LogTestConfig::Load(directory, "Appender.Capture = 200,1,0\nLogger.root = 3,Capture\nLogger.sql.sql = 2,Capture\n", [&environment](std::string const& name) -> std::optional<std::string>
    {
        auto const it = environment.find(name);
        return it == environment.end() ? std::nullopt : std::optional<std::string>(it->second);
    });
    LogConfigResult result;
    LogSettings const settings = LogConfig::Read(*config, result);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    ASSERT_NE(settings.FindLogger("sql.sql"), nullptr);
    EXPECT_EQ(settings.FindLogger("sql.sql")->Level, LogLevel::Warn);
    EXPECT_TRUE(settings.AsyncEnable);
}

TEST(LogConfigTest, OverrideCanAddLogger)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("app.conf", "# Project Ambrose by Imjustchico\n# Logging test configuration.\nAppender.Capture = 200,1,0\nLogger.root = 3,Capture\n");
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file, {}, { { "Logger.network", "1,Capture" } }).Succeeded());
    LogConfigResult result;
    LogSettings const settings = LogConfig::Read(config, result);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    ASSERT_NE(settings.FindLogger("network"), nullptr);
    EXPECT_EQ(settings.FindLogger("network")->Level, LogLevel::Trace);
    EXPECT_EQ(settings.Loggers.front().Name, "root");
}

TEST(LogConfigTest, ReadingSettingsNeverRaisesConfigWarnings)
{
    LogTestDirectory directory;
    std::unique_ptr<ConfigMgr> const config = LogTestConfig::Load(directory, "Log.Async.Enable = maybe\nAppender.Capture = 200,1,0\nLogger.root = 3,Capture\n");
    LogConfigResult result;
    LogConfig::Read(*config, result);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(config->TakeWarnings().empty());
}

TEST(LogConfigTest, FailedApplyKeepsPreviousConfiguration)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n");
    uint64 const generation = harness.GetLog().GetGeneration();

    LogSettings broken = LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n");
    broken.Loggers.clear();
    LogConfigResult const result = harness.GetLog().Apply(std::move(broken));
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(harness.GetLog().GetGeneration(), generation);

    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.test", "still {}", "here");
    EXPECT_EQ(harness.Store().Texts("Capture"), std::vector<std::string>{ "still here" });
}
