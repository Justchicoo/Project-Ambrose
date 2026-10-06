/*
 * Project Ambrose by Imjustchico
 * Checks the start step a process reports: nothing is reported until a step names itself, the latest report is the one held with the time it asked for counted from when it reported, clearing forgets it, a listener hears every report until it is taken away, and the line for a step names the app, the seconds and the stage.
 */

#include "StartProgress.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

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

TEST(StartProgressTest, AListenerHearsEachReportUntilItIsTakenAway)
{
    std::vector<std::pair<std::string, long long>> heard;
    StartProgress::SetListener([&heard](std::string_view stage, std::chrono::seconds allowance) { heard.emplace_back(std::string(stage), allowance.count()); });
    StartProgress::Report("building the type dump", std::chrono::seconds(900));
    StartProgress::Report("extracting zones", std::chrono::seconds(120));
    StartProgress::SetListener({});
    StartProgress::Report("writing zones", std::chrono::seconds(60));
    StartProgress::Clear();

    ASSERT_EQ(heard.size(), 2u);
    EXPECT_EQ(heard[0], std::make_pair(std::string("building the type dump"), 900LL));
    EXPECT_EQ(heard[1], std::make_pair(std::string("extracting zones"), 120LL));
}

TEST(StartProgressTest, TheStepLineNamesTheAppTheSecondsAndTheStage)
{
    std::string const line = StartProgress::StepText("gameserver", "building the type dump for revision r1", std::chrono::seconds(960));
    EXPECT_EQ(line, "gameserver start step, up to 960 s: building the type dump for revision r1");
    EXPECT_EQ(line.rfind(StartProgress::StepPrefix("gameserver"), 0), 0u);
}
