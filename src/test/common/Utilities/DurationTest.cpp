/*
 * Project Ambrose by Imjustchico
 * Tests operator duration parsing: single and combined units, bare seconds, case, refusal of zero, empty text, unknown units, dangling units and overflow, and printing a duration back for an operator to read.
 */

#include "Duration.h"

#include <gtest/gtest.h>

TEST(DurationTest, ParsesUnitsAndCombinations)
{
    EXPECT_EQ(Ambrose::ParseDuration("90"), Seconds(90));
    EXPECT_EQ(Ambrose::ParseDuration("45s"), Seconds(45));
    EXPECT_EQ(Ambrose::ParseDuration("30m"), Minutes(30));
    EXPECT_EQ(Ambrose::ParseDuration("12H"), Hours(12));
    EXPECT_EQ(Ambrose::ParseDuration("7d"), Hours(7 * 24));
    EXPECT_EQ(Ambrose::ParseDuration("2w"), Hours(14 * 24));
    EXPECT_EQ(Ambrose::ParseDuration("1d12h30m5"), Seconds(86400 + 12 * 3600 + 30 * 60 + 5));
}

TEST(DurationTest, RejectsUnusableText)
{
    for (std::string_view const text : { "", "0", "0d", "d", "5x", "1.5h", "-1d", "12 h", "h12" })
        EXPECT_FALSE(Ambrose::ParseDuration(text)) << text;
    EXPECT_FALSE(Ambrose::ParseDuration("99999999999999999999"));
    EXPECT_FALSE(Ambrose::ParseDuration("9223372036854775807w"));
    EXPECT_EQ(Ambrose::ParseDuration("9223372036854775807"), Seconds(9223372036854775807LL));
}

TEST(DurationTest, PrintsOnlyTheUnitsThatMatter)
{
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(0)), "0s");
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(-5)), "0s");
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(45)), "45s");
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(605)), "10m 5s");
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(3600)), "1h 0m 0s");
    EXPECT_EQ(Ambrose::FormatDuration(Seconds(90061)), "1d 1h 1m 1s");
}
