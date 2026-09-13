/*
 * Project Ambrose by Imjustchico
 * Tests level, type, flag, color and byte size names and parsing.
 */

#include "LogCommon.h"

#include <gtest/gtest.h>

using namespace Ambrose::Logging;

TEST(LogCommonTest, LevelsFollowAzerothCoreNumbering)
{
    EXPECT_EQ(ParseLogLevel("0"), LogLevel::Disabled);
    EXPECT_EQ(ParseLogLevel("1"), LogLevel::Trace);
    EXPECT_EQ(ParseLogLevel("2"), LogLevel::Debug);
    EXPECT_EQ(ParseLogLevel("3"), LogLevel::Info);
    EXPECT_EQ(ParseLogLevel("4"), LogLevel::Warn);
    EXPECT_EQ(ParseLogLevel("5"), LogLevel::Error);
    EXPECT_EQ(ParseLogLevel("6"), LogLevel::Fatal);
    EXPECT_EQ(ParseLogLevel("WARN"), LogLevel::Warn);
    EXPECT_EQ(ParseLogLevel("Warning"), LogLevel::Warn);
    EXPECT_EQ(ParseLogLevel(" info "), LogLevel::Info);
    EXPECT_EQ(ParseLogLevel("Disabled"), LogLevel::Disabled);
    EXPECT_FALSE(ParseLogLevel("7").has_value());
    EXPECT_FALSE(ParseLogLevel("-1").has_value());
    EXPECT_FALSE(ParseLogLevel("verbose").has_value());
    EXPECT_FALSE(ParseLogLevel("").has_value());
    EXPECT_EQ(GetLogLevelName(LogLevel::Error), "ERROR");
    EXPECT_EQ(GetLogLevelPaddedName(LogLevel::Info), "INFO ");
    EXPECT_EQ(GetLogLevelPaddedName(LogLevel::Fatal), "FATAL");
}

TEST(LogCommonTest, AppenderTypesParseNumbersAndNames)
{
    EXPECT_EQ(ParseAppenderType("1"), AppenderType::Console);
    EXPECT_EQ(ParseAppenderType("file"), AppenderType::File);
    EXPECT_EQ(ParseAppenderType("Stream"), AppenderType::Stream);
    EXPECT_EQ(ParseAppenderType("DB"), AppenderType::DB);
    EXPECT_EQ(ParseAppenderType("200"), static_cast<AppenderType>(200));
    EXPECT_FALSE(ParseAppenderType("0").has_value());
    EXPECT_FALSE(ParseAppenderType("256").has_value());
    EXPECT_FALSE(ParseAppenderType("Printer").has_value());
    EXPECT_EQ(GetAppenderTypeName(AppenderType::File), "File");
}

TEST(LogCommonTest, FlagsRejectUnknownBits)
{
    EXPECT_EQ(ParseAppenderFlags("0x3F"), static_cast<AppenderFlags>(0x3F));
    EXPECT_EQ(ParseAppenderFlags("63"), static_cast<AppenderFlags>(0x3F));
    EXPECT_EQ(ParseAppenderFlags("7"), AppenderFlags::PrefixTimestamp | AppenderFlags::PrefixLevel | AppenderFlags::PrefixCategory);
    EXPECT_EQ(ParseAppenderFlags("0"), AppenderFlags::None);
    EXPECT_FALSE(ParseAppenderFlags("0x40").has_value());
    EXPECT_FALSE(ParseAppenderFlags("0x80").has_value());
    EXPECT_FALSE(ParseAppenderFlags("64").has_value());
    EXPECT_FALSE(ParseAppenderFlags("seven").has_value());
}

TEST(LogCommonTest, ColorsParseZeroToFifteen)
{
    EXPECT_EQ(ParseConsoleColor("0"), ConsoleColor::Black);
    EXPECT_EQ(ParseConsoleColor("9"), ConsoleColor::LightRed);
    EXPECT_EQ(ParseConsoleColor("15"), ConsoleColor::Default);
    EXPECT_FALSE(ParseConsoleColor("16").has_value());
    EXPECT_FALSE(ParseConsoleColor("red").has_value());
}

TEST(LogCommonTest, ByteSizesParseSuffixes)
{
    EXPECT_EQ(ParseByteSize("0"), 0u);
    EXPECT_EQ(ParseByteSize("1K"), 1024u);
    EXPECT_EQ(ParseByteSize("64m"), 64u * 1024 * 1024);
    EXPECT_EQ(ParseByteSize("1G"), uint64{ 1 } << 30);
    EXPECT_EQ(ParseByteSize("4096"), 4096u);
    EXPECT_FALSE(ParseByteSize("12Q").has_value());
    EXPECT_FALSE(ParseByteSize("K").has_value());
    EXPECT_FALSE(ParseByteSize("99999999999999999999G").has_value());
    EXPECT_FALSE(ParseByteSize("17179869184G").has_value());
}

TEST(LogCommonTest, IsLevelEnabledTreatsZeroAsDisabled)
{
    EXPECT_TRUE(IsLevelEnabled(LogLevel::Info, LogLevel::Info));
    EXPECT_TRUE(IsLevelEnabled(LogLevel::Info, LogLevel::Fatal));
    EXPECT_FALSE(IsLevelEnabled(LogLevel::Info, LogLevel::Debug));
    EXPECT_FALSE(IsLevelEnabled(LogLevel::Disabled, LogLevel::Fatal));
    EXPECT_FALSE(IsLevelEnabled(LogLevel::Trace, LogLevel::Disabled));
}

TEST(LogCommonTest, CategoryWithinMatchesAtDotBoundary)
{
    EXPECT_TRUE(IsCategoryWithin("sql.sql", "sql"));
    EXPECT_TRUE(IsCategoryWithin("sql", "sql"));
    EXPECT_TRUE(IsCategoryWithin("anything", ""));
    EXPECT_FALSE(IsCategoryWithin("sqlx", "sql"));
    EXPECT_FALSE(IsCategoryWithin("sq", "sql"));
    EXPECT_FALSE(IsCategoryWithin("server.sql", "sql"));
}
