/*
 * Project Ambrose by Imjustchico
 * Tests exact invariant LocationString formatting, decimal parsing under any process locale, and named locations.
 */

#include "LocationString.h"

#include <gtest/gtest.h>

#include <locale>

TEST(LocationStringTest, FormatsAndParsesTheCapturedCoordinates)
{
    LocationString const location = LocationString::CoordinatesOf(-32.0f, -552.0f, -28.0f, 6.350083f);
    EXPECT_EQ(location.Format(), "-32,-552,-28,6.350083");
    std::optional<LocationString> const parsed = LocationString::Parse("-32,-552,-28,6.350083");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->IsNamed());
    EXPECT_EQ(parsed->Coordinates, location.Coordinates);
}

TEST(LocationStringTest, ParsesDecimalPointsIndependentlyOfTheProcessLocale)
{
    std::locale const previous = std::locale();
    try
    {
        std::locale::global(std::locale("fr-FR"));
    }
    catch (std::runtime_error const&)
    {
        GTEST_SKIP() << "fr-FR locale is not installed on this machine";
    }
    std::optional<LocationString> const parsed = LocationString::Parse("857.9,5730.8,-18.09,1.40");
    std::locale::global(previous);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FLOAT_EQ(parsed->Coordinates[0], 857.9f);
    EXPECT_FLOAT_EQ(parsed->Coordinates[1], 5730.8f);
    EXPECT_FLOAT_EQ(parsed->Coordinates[2], -18.09f);
    EXPECT_FLOAT_EQ(parsed->Coordinates[3], 1.40f);
}

TEST(LocationStringTest, PreservesNamedLocations)
{
    std::optional<LocationString> const parsed = LocationString::Parse("Start");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->IsNamed());
    EXPECT_EQ(parsed->Format(), "Start");
}
