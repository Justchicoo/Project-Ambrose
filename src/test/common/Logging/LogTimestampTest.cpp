/*
 * Project Ambrose by Imjustchico
 * Tests timestamp formats, UTC, second rollover and concurrent formatting.
 */

#include "LogTimestamp.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <atomic>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    std::chrono::system_clock::time_point AtMilliseconds(int64 milliseconds)
    {
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(milliseconds));
    }
}

TEST(LogTimestampTest, PrefixFormatMatchesPattern)
{
    std::string const local(LogTimestamp::FormatPrefix(std::chrono::system_clock::now(), false));
    EXPECT_EQ(local.size(), LogTimestamp::PrefixLength);
    EXPECT_TRUE(std::regex_match(local, std::regex(R"(\d{4}-\d{2}-\d{2}_\d{2}:\d{2}:\d{2}\.\d{3})"))) << local;
}

TEST(LogTimestampTest, UtcFormatsKnownEpoch)
{
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(1700000000123), true), "2023-11-14_22:13:20.123");
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(0), true), "1970-01-01_00:00:00.000");
}

TEST(LogTimestampTest, FileNameFormatUsesDashes)
{
    std::array<char, LogTimestamp::FileNameLength> const name = LogTimestamp::FormatFileName(AtMilliseconds(1700000000999), true);
    EXPECT_EQ(std::string(name.begin(), name.end()), "2023-11-14_22-13-20");
}

TEST(LogTimestampTest, SecondRolloverRecomputes)
{
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(1700000000999), true), "2023-11-14_22:13:20.999");
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(1700000001000), true), "2023-11-14_22:13:21.000");
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(1700000000500), true), "2023-11-14_22:13:20.500");
}

TEST(LogTimestampTest, LocalAndUtcCachesAreIndependent)
{
    std::chrono::system_clock::time_point const time = AtMilliseconds(1700000000123);
    std::string const utc(LogTimestamp::FormatPrefix(time, true));
    std::tm parts{};
    ASSERT_TRUE(LogTimestamp::BreakDown(1700000000, false, parts));
    std::string const expectedLocal = fmt::format("{:04}-{:02}-{:02}_{:02}:{:02}:{:02}.123", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    EXPECT_EQ(LogTimestamp::FormatPrefix(time, false), expectedLocal);
    EXPECT_EQ(LogTimestamp::FormatPrefix(time, true), utc);
}

TEST(LogTimestampTest, MillisecondsFloorBeforeEpoch)
{
    EXPECT_EQ(LogTimestamp::FormatPrefix(AtMilliseconds(-1), true), "1969-12-31_23:59:59.999");
}

TEST(LogTimestampTest, ConcurrentFormattingIsConsistent)
{
    std::atomic<int> mismatches{ 0 };
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread)
    {
        threads.emplace_back([thread, &mismatches]
        {
            for (int i = 0; i < 2000; ++i)
            {
                int64 const milliseconds = 1700000000000 + int64{ thread } * 7919 + int64{ i } * 997;
                std::tm parts{};
                if (!LogTimestamp::BreakDown(static_cast<std::time_t>(milliseconds / 1000), true, parts))
                {
                    ++mismatches;
                    continue;
                }
                std::string const expected = fmt::format("{:04}-{:02}-{:02}_{:02}:{:02}:{:02}.{:03}", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec, milliseconds % 1000);
                if (LogTimestamp::FormatPrefix(AtMilliseconds(milliseconds), true) != expected)
                    ++mismatches;
            }
        });
    }
    for (std::thread& thread : threads)
        thread.join();
    EXPECT_EQ(mismatches.load(), 0);
}
