/*
 * Project Ambrose by Imjustchico
 * Tests Server.log in LogsDir with its prefix, modes, backups, rotation, pruning, flush policy and open failures.
 */

#include "AppenderFile.h"
#include "Log.h"
#include "LogFileRegistry.h"
#include "LogTestHarness.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    std::string Utf8(std::filesystem::path const& path)
    {
        return ConfigMgr::PathToUtf8(path);
    }

    class SLogFileTest : public testing::TestWithParam<bool>
    {
    protected:
        void TearDown() override
        {
            sLog.Reset();
        }

        LogTestDirectory _directory;
    };

    std::vector<std::string> NumberedLines(LogTestDirectory const& directory, std::vector<std::filesystem::path> const& files)
    {
        std::vector<std::string> lines;
        for (std::filesystem::path const& file : files)
            for (std::string& line : directory.ReadLines(file))
                lines.push_back(std::move(line));
        return lines;
    }
}

TEST_P(SLogFileTest, WritesServerLogInLogsDirWithPrefix)
{
    std::filesystem::path const logsDir = _directory.Path() / "nested" / "logs";
    ASSERT_FALSE(std::filesystem::exists(logsDir));
    LogConfigResult const applied = sLog.Apply(LogTestConfig::Settings(fmt::format("LogsDir = {}\nLog.Async.Enable = {}\nAppender.Server = 2,2,7,Server.log,w\nLogger.root = 3,Server\n", Utf8(logsDir), GetParam() ? 1 : 0)));
    ASSERT_TRUE(applied.Succeeded()) << LogTestConfig::Describe(applied);
    EXPECT_TRUE(std::filesystem::is_directory(logsDir));

    LOG_INFO("server.gameserver", "Hello {}", "wizard");
    LOG_DEBUG("server.gameserver", "not written");
    ASSERT_TRUE(sLog.Flush());

    std::filesystem::path const file = logsDir / "Server.log";
    std::vector<std::string> const lines = _directory.ReadLines(file);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_TRUE(std::regex_match(lines[0], std::regex(R"(^\d{4}-\d{2}-\d{2}_\d{2}:\d{2}:\d{2}\.\d{3} INFO  \[server\.gameserver\] Hello wizard$)"))) << lines[0];
    EXPECT_EQ(_directory.ReadBytes(file).find('\r'), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(SyncAndAsync, SLogFileTest, testing::Bool());

TEST(AppenderFileTest, ModeWTruncatesOnFirstOpenOnly)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("logs/Server.log", "old line\n");
    std::string const logsDir = Utf8(directory.Path() / "logs");
    {
        LogTestHarness harness;
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Server.log,w\nLogger.root = 2,Server\n", logsDir));
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "one");
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,1,0,Server.log,w,0,0,10\nLogger.root = 2,Server\n", logsDir));
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "two");
        harness.GetLog().Flush();
    }
    EXPECT_EQ(directory.ReadLines(file), (std::vector<std::string>{ "one", "two" }));
}

TEST(AppenderFileTest, ModeAAppendsAcrossInstances)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("logs/Append.log", "kept\n");
    std::string const body = fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Append.log,a\nLogger.root = 2,Server\n", Utf8(directory.Path() / "logs"));
    for (int round = 0; round < 2; ++round)
    {
        LogTestHarness harness;
        harness.ApplyOrFail(body);
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "round {}", round);
    }
    EXPECT_EQ(directory.ReadLines(file), (std::vector<std::string>{ "kept", "round 0", "round 1" }));
}

TEST(AppenderFileTest, BackupFlagRenamesExistingFile)
{
    LogTestDirectory directory;
    directory.Write("logs/Backup.log", "previous run\n");
    std::filesystem::path const logsDir = directory.Path() / "logs";
    {
        LogTestHarness harness;
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0x10,Backup.log,w\nLogger.root = 2,Server\n", Utf8(logsDir)));
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "this run");
    }
    std::vector<std::filesystem::path> const files = directory.ListFiles(logsDir);
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(directory.ReadLines(logsDir / "Backup.log"), std::vector<std::string>{ "this run" });
    std::filesystem::path const backup = files[0].filename() == "Backup.log" ? files[1] : files[0];
    EXPECT_TRUE(std::regex_match(Utf8(backup.filename()), std::regex(R"(^Backup_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.log$)"))) << Utf8(backup);
    EXPECT_EQ(directory.ReadLines(backup), std::vector<std::string>{ "previous run" });
}

TEST(AppenderFileTest, TimestampFileNameFlag)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    {
        LogTestHarness harness;
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0x08,Stamped.log,w\nLogger.root = 2,Server\n", Utf8(logsDir)));
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "stamped");
    }
    std::vector<std::filesystem::path> const files = directory.ListFiles(logsDir);
    ASSERT_EQ(files.size(), 1u);
    EXPECT_TRUE(std::regex_match(Utf8(files[0].filename()), std::regex(R"(^Stamped_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(_\d+)?\.log$)"))) << Utf8(files[0]);
    EXPECT_EQ(directory.ReadLines(files[0]), std::vector<std::string>{ "stamped" });
}

TEST(AppenderFileTest, RotatesAtMaxSizeAndPrunesBackups)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    {
        LogTestHarness harness;
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Rot = 2,2,0,Rot.log,w,1K,2\nLogger.root = 2,Rot\n", Utf8(logsDir)));
        for (int i = 1; i <= 60; ++i)
            AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "line {:03} {}", i, std::string(60, 'r'));
        std::shared_ptr<AppenderFile> const appender = std::dynamic_pointer_cast<AppenderFile>(harness.GetLog().GetAppender("Rot"));
        ASSERT_NE(appender, nullptr);
        EXPECT_GE(appender->GetFile()->GetRotationCount(), 3u);
    }
    std::vector<std::filesystem::path> files = directory.ListFiles(logsDir);
    ASSERT_EQ(files.size(), 3u);
    std::filesystem::path const active = logsDir / "Rot.log";
    std::vector<std::filesystem::path> backups;
    for (std::filesystem::path const& file : files)
    {
        EXPECT_LE(std::filesystem::file_size(file), 1024u + 70u) << Utf8(file);
        if (file != active)
            backups.push_back(file);
    }
    auto const backupKey = [](std::filesystem::path const& path)
    {
        std::string const stem = Utf8(path.stem());
        std::string const stamp = stem.substr(4, 19);
        int const collision = stem.size() > 23 ? std::stoi(stem.substr(24)) : 0;
        return std::make_pair(stamp, collision);
    };
    std::sort(backups.begin(), backups.end(), [&backupKey](std::filesystem::path const& left, std::filesystem::path const& right)
    {
        return backupKey(left) < backupKey(right);
    });
    backups.push_back(active);
    std::vector<std::string> const lines = NumberedLines(directory, backups);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.back().substr(0, 8), "line 060");
    int const first = std::stoi(lines.front().substr(5, 3));
    for (std::size_t i = 0; i < lines.size(); ++i)
        EXPECT_EQ(std::stoi(lines[i].substr(5, 3)), first + static_cast<int>(i)) << lines[i];
}

TEST(AppenderFileTest, CollisionSuffixSortsNumerically)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    for (std::string const suffix : { "", "_1", "_2", "_10" })
        directory.Write("logs/Server_2020-01-01_00-00-00" + suffix + ".log", "x\n");
    directory.Write("logs/Server_notes.log", "unrelated\n");
    {
        LogTestHarness harness;
        harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Server.log,a,1M,2\nLogger.root = 2,Server\n", Utf8(logsDir)));
    }
    EXPECT_FALSE(std::filesystem::exists(logsDir / "Server_2020-01-01_00-00-00.log"));
    EXPECT_FALSE(std::filesystem::exists(logsDir / "Server_2020-01-01_00-00-00_1.log"));
    EXPECT_TRUE(std::filesystem::exists(logsDir / "Server_2020-01-01_00-00-00_2.log"));
    EXPECT_TRUE(std::filesystem::exists(logsDir / "Server_2020-01-01_00-00-00_10.log"));
    EXPECT_TRUE(std::filesystem::exists(logsDir / "Server_notes.log"));
}

TEST(AppenderFileTest, FlushIntervalDefersButErrorFlushes)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Slow = 2,2,0,Slow.log,a,0,0,60000\nLogger.root = 2,Slow\n", Utf8(logsDir)));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "buffered");
    EXPECT_EQ(directory.ReadBytes(logsDir / "Slow.log"), "");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server", "urgent");
    EXPECT_EQ(directory.ReadLines(logsDir / "Slow.log"), (std::vector<std::string>{ "buffered", "urgent" }));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "later");
    EXPECT_EQ(directory.ReadLines(logsDir / "Slow.log").size(), 2u);
    ASSERT_TRUE(harness.GetLog().Flush());
    EXPECT_EQ(directory.ReadLines(logsDir / "Slow.log").size(), 3u);
}

TEST(AppenderFileTest, SyncFlushIntervalWritesWithoutAnotherLine)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Timed = 2,2,0,Timed.log,a,0,0,50\nLogger.root = 2,Timed\n", Utf8(logsDir)));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "realm online");
    std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (directory.ReadBytes(logsDir / "Timed.log").empty() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(directory.ReadLines(logsDir / "Timed.log"), std::vector<std::string>{ "realm online" });
}

TEST(AppenderFileTest, ReloadAppliesLogsDirAndUtcToKeptAppenders)
{
    LogTestDirectory directory;
    LogTestHarness harness;
    auto body = [&directory](char const* folder, int utc)
    {
        return fmt::format("LogsDir = {}\nLog.Utc = {}\nAppender.Server = 2,2,0,Server.log,a\nLogger.root = 2,Server\n", Utf8(directory.Path() / folder), utc);
    };
    harness.ApplyOrFail(body("first", 0));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "in first");
    harness.ApplyOrFail(body("second", 0));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "in second");
    std::shared_ptr<Appender> const before = harness.GetLog().GetAppender("Server");
    harness.ApplyOrFail(body("second", 1));
    std::shared_ptr<Appender> const utc = harness.GetLog().GetAppender("Server");
    harness.ApplyOrFail(body("second", 1));
    ASSERT_TRUE(harness.GetLog().Flush());
    EXPECT_EQ(directory.ReadLines(directory.Path() / "first" / "Server.log"), std::vector<std::string>{ "in first" });
    EXPECT_EQ(directory.ReadLines(directory.Path() / "second" / "Server.log"), std::vector<std::string>{ "in second" });
    EXPECT_NE(before, utc);
    EXPECT_EQ(harness.GetLog().GetAppender("Server"), utc);
}

TEST(AppenderFileTest, OpenFailureIsReportedAndNotApplied)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    std::filesystem::create_directories(logsDir / "Server.log");
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n");
    uint64 const generation = harness.GetLog().GetGeneration();
    LogConfigResult const result = harness.Apply(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Server.log\nLogger.root = 2,Server\n", Utf8(logsDir)));
    ASSERT_FALSE(result.Succeeded());
    EXPECT_NE(result.Errors[0].Message.find("Appender.Server: cannot open"), std::string::npos) << LogTestConfig::Describe(result);
    EXPECT_EQ(harness.GetLog().GetGeneration(), generation);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "still captured");
    EXPECT_EQ(harness.Store().Count("Capture"), 1u);
}

TEST(AppenderFileTest, NonAsciiPathAndLogsDir)
{
    LogTestDirectory directory;
    std::string const logsDir = Utf8(directory.Path()) + "/Ravenwood_\xC3\xA9";
    {
        LogTestHarness harness;
        harness.ApplyOrFail("LogsDir = " + logsDir + "\nAppender.Server = 2,2,0,\xD0\x96\xD1\x83\xD1\x80\xD0\xBD\xD0\xB0\xD0\xBB/Server.log\nLogger.root = 2,Server\n");
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "caf\xC3\xA9");
    }
    std::filesystem::path const file = directory.Path() / std::filesystem::path(u8"Ravenwood_\u00E9") / std::filesystem::path(u8"\u0416\u0443\u0440\u043D\u0430\u043B") / "Server.log";
    ASSERT_TRUE(std::filesystem::exists(file));
    EXPECT_EQ(directory.ReadLines(file), std::vector<std::string>{ "caf\xC3\xA9" });
}

TEST(AppenderFileTest, ChangedDefinitionSharesHandle)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Shared.log,w\nLogger.root = 2,Server\n", Utf8(logsDir)));
    std::shared_ptr<AppenderFile> const before = std::dynamic_pointer_cast<AppenderFile>(harness.GetLog().GetAppender("Server"));
    for (int i = 0; i < 1000; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Server = 2,2,0,Shared.log,w,64M\nLogger.root = 2,Server\n", Utf8(logsDir)));
    std::shared_ptr<AppenderFile> const after = std::dynamic_pointer_cast<AppenderFile>(harness.GetLog().GetAppender("Server"));
    ASSERT_NE(before, nullptr);
    ASSERT_NE(after, nullptr);
    EXPECT_NE(before, after);
    EXPECT_EQ(before->GetFile(), after->GetFile());
    for (int i = 1000; i < 2000; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    ASSERT_TRUE(harness.GetLog().Flush());
    std::vector<std::string> const lines = directory.ReadLines(logsDir / "Shared.log");
    ASSERT_EQ(lines.size(), 2000u);
    for (int i = 0; i < 2000; ++i)
        EXPECT_EQ(lines[static_cast<std::size_t>(i)], std::to_string(i));
}

#ifdef _WIN32
TEST(AppenderFileTest, RotationRenameFailureContinuesInPlace)
{
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    harness.ApplyOrFail(fmt::format("LogsDir = {}\nAppender.Rot = 2,2,0,Locked.log,w,1K\nLogger.root = 2,Rot\n", Utf8(logsDir)));
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "first {}", std::string(100, 'a'));
    HANDLE const reader = ::CreateFileW((logsDir / "Locked.log").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(reader, INVALID_HANDLE_VALUE);
    for (int i = 0; i < 30; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "line {} {}", i, std::string(60, 'b'));
    ::CloseHandle(reader);
    ASSERT_TRUE(harness.GetLog().Flush());
    std::string const content = directory.ReadBytes(logsDir / "Locked.log");
    EXPECT_NE(content.find("cannot rotate"), std::string::npos);
    EXPECT_NE(content.find("line 29"), std::string::npos);
    EXPECT_EQ(directory.ListFiles(logsDir).size(), 1u);
}
#endif
