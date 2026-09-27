/*
 * Project Ambrose by Imjustchico
 * Checks the start step a process reports: nothing is reported until a step names itself, the latest report is the one held with the time it asked for counted from when it reported, and clearing forgets it.
 */

#include "StartProgress.h"

#include <gtest/gtest.h>

#include <chrono>

TEST(StartProgressTest, TheLatestStepIsHeldWithItsTimeUntilCleared)
{
    StartProgress::Clear();
    EXPECT_FALSE(StartProgress::Current().has_value());

    auto const before = std::chrono::system_clock::now();
    StartProgress::Report("building the type dump", std::chrono::seconds(900));
    StartProgress::Report("extracting zones", std::chrono::seconds(120));
    auto const after = std::chrono::system_clock::now();

    std::optional<StartStep> const step = StartProgress::Current();
    ASSERT_TRUE(step.has_value());
    EXPECT_EQ(step->Stage, "extracting zones");
    EXPECT_GE(step->Until, before + std::chrono::seconds(120));
    EXPECT_LE(step->Until, after + std::chrono::seconds(120));

    StartProgress::Clear();
    EXPECT_FALSE(StartProgress::Current().has_value());
}
