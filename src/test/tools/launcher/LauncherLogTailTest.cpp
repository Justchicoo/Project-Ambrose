/*
 * Project Ambrose by Imjustchico
 * Tests following the client's own log: a poll before the file exists hands over nothing and the next one reads it, appended lines arrive as they are written without their carriage returns, a line the client has not finished waits until it is finished or Finish hands it over, and a line longer than the limit is cut instead of growing without bound.
 */

#include "LogTail.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    void Append(std::filesystem::path const& file, std::string const& text)
    {
        std::ofstream stream(file, std::ios::binary | std::ios::app);
        stream << text;
    }
}

TEST(LauncherLogTailTest, HandsOverEveryFinishedLineAsItIsWritten)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "WizardClient.log";
    std::vector<std::string> lines;
    LogTail tail(file, [&lines](std::string_view line) { lines.emplace_back(line); });
    tail.Poll();
    EXPECT_TRUE(lines.empty());

    Append(file, "[INFO] starting\r\n[ERRO] something\n");
    tail.Poll();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "[INFO] starting");
    EXPECT_EQ(lines[1], "[ERRO] something");

    Append(file, "[WARN] later\n");
    tail.Poll();
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[2], "[WARN] later");
}

TEST(LauncherLogTailTest, AnUnfinishedLineWaitsUntilFinish)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "WizardClient.log";
    std::vector<std::string> lines;
    LogTail tail(file, [&lines](std::string_view line) { lines.emplace_back(line); });
    Append(file, "[INFO] the client stopped here");
    tail.Poll();
    EXPECT_TRUE(lines.empty());
    tail.Finish();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "[INFO] the client stopped here");
}

TEST(LauncherLogTailTest, CutsALineNoProgramShouldWrite)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "WizardClient.log";
    std::vector<std::string> lines;
    LogTail tail(file, [&lines](std::string_view line) { lines.emplace_back(line); });
    Append(file, std::string(LogTail::MaxLineBytes + 100, 'a') + "\n");
    tail.Poll();
    tail.Finish();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].size(), LogTail::MaxLineBytes);
    EXPECT_EQ(lines[1].size(), 100u);
}
