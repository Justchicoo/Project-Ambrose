/*
 * Project Ambrose by Imjustchico
 * Tests that std::exit without Shutdown still drains the async queue and file buffers.
 */

#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>

namespace
{
    constexpr char const* ExitDirectoryVariable = "AMBROSE_TEST_LOG_EXIT_DIR";

    std::filesystem::path ExitDirectory()
    {
        std::optional<std::string> const existing = Ambrose::GetEnv(ExitDirectoryVariable);
        if (existing && !existing->empty())
            return LogConfig::Utf8Path(*existing);
        std::random_device device;
        std::filesystem::path const directory = std::filesystem::path(testing::TempDir()) / ("ambrose-log-exit-" + std::to_string(device()) + "-" + std::to_string(device()));
        std::filesystem::create_directories(directory);
        Ambrose::SetEnv(ExitDirectoryVariable, ConfigMgr::PathToUtf8(directory));
        return directory;
    }

    [[noreturn]] void LogThenExitWithoutShutdown(std::filesystem::path const& directory)
    {
        LogSettings const settings = LogTestConfig::Settings(fmt::format("LogsDir = {}\nLog.Async.Enable = 1\nAppender.Exit = 2,2,7,Exit.log,w,0,0,60000\nLogger.root = 2,Exit\n", ConfigMgr::PathToUtf8(directory)));
        if (!sLog.Apply(settings).Succeeded())
            std::exit(3);
        for (int i = 0; i < 1000; ++i)
            LOG_INFO("server.exit", "line {}", i);
        std::exit(0);
    }
}

TEST(LogExitTest, StdExitDrainsAsyncQueueAndFileBuffer)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    std::filesystem::path const directory = ExitDirectory();
    std::filesystem::path const file = directory / "Exit.log";
    EXPECT_EXIT(LogThenExitWithoutShutdown(directory), testing::ExitedWithCode(0), "");

    std::ifstream stream(file, std::ios::binary);
    std::string const content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    std::size_t const lines = static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n'));
    EXPECT_EQ(lines, 1000u);
    EXPECT_NE(content.find("[server.exit] line 999\n"), std::string::npos);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    Ambrose::UnsetEnv(ExitDirectoryVariable);
}
