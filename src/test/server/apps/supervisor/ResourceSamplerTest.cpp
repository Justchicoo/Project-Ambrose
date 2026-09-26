/*
 * Project Ambrose by Imjustchico
 * Checks what the sampler promises: the first reading of a process writes everything but the processor share, because a share needs two readings, the second works the share out against the time that actually passed rather than the time a round was meant to take, an app with no process is skipped so its graph carries a gap rather than zeroes, an app that stops and starts again is measured against its new process rather than against a total that belongs to the old one, every app is written under its own name, a round costs few enough microseconds per app to sit on a timer beside twenty of them, judged by the median of two hundred rounds timed one by one so the few a busy machine preempts are not counted as the sampler's own work, the series a graph would draw for a process holding half a core agrees within fifteen points with what that process measured on its own clock, which is wide enough that a machine busy building something else does not fail it and narrow enough that a share worked out per machine rather than per core still does, and an app that stops leaves a gap at the end of its own graph while the app beside it goes on being written.
 */

#include "ChildProcess.h"
#include "LogTestConfig.h"
#include "ResourceSampler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>
#include <thread>
#include <string>
#include <vector>

namespace
{
    using namespace Ambrose;

    AppSnapshot Running(std::string name, int64 processId)
    {
        AppSnapshot app;
        app.Name = std::move(name);
        app.ProcessId = processId;
        app.State = AppState::Running;
        return app;
    }

    AppSnapshot Stopped(std::string name)
    {
        AppSnapshot app;
        app.Name = std::move(name);
        app.State = AppState::Offline;
        return app;
    }

    ProcessSnapshot Used(uint64 cpuMicroseconds, uint64 residentBytes = 1024 * 1024)
    {
        ProcessSnapshot snapshot;
        snapshot.CpuMicroseconds = cpuMicroseconds;
        snapshot.ResidentBytes = residentBytes;
        snapshot.ThreadCount = 8;
        snapshot.OpenHandles = 42;
        return snapshot;
    }

    std::size_t PresentIn(std::vector<SeriesPoint> const& points)
    {
        std::size_t present = 0;
        for (SeriesPoint const& point : points)
            if (point.Present)
                ++present;
        return present;
    }
}

TEST(ResourceSamplerTest, TheFirstReadingWritesEverythingButTheProcessorShare)
{
    SeriesStore store;
    ResourceSampler sampler(store, [](uint32) { return Used(5000000); });

    std::vector<ResourceSampler::Reading> const readings = sampler.Sample({ Running("gameserver", 1234) }, 0);
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_TRUE(readings[0].Sampled);
    EXPECT_DOUBLE_EQ(readings[0].CpuPercent, 0.0);

    EXPECT_TRUE(store.Has("gameserver", ResourceSampler::MemorySeries));
    EXPECT_TRUE(store.Has("gameserver", ResourceSampler::ThreadSeries));
    EXPECT_FALSE(store.Has("gameserver", ResourceSampler::CpuSeries))
        << "one total says nothing about a share, and inventing one would read as the app having used the machine since it booted";
}

TEST(ResourceSamplerTest, TheSecondReadingWorksTheShareOutAgainstTheTimeThatPassed)
{
    SeriesStore store;
    uint64 cpu = 0;
    ResourceSampler sampler(store, [&cpu](uint32) { return Used(cpu); });

    sampler.Sample({ Running("gameserver", 1234) }, 0);
    cpu = 1000000;
    std::vector<ResourceSampler::Reading> const second = sampler.Sample({ Running("gameserver", 1234) }, 2000);

    ASSERT_EQ(second.size(), 1u);
    EXPECT_TRUE(second[0].Sampled);
    EXPECT_NEAR(second[0].CpuPercent, 50.0, 0.001) << "one second of processor time over two seconds of wall clock is half a core";
    EXPECT_TRUE(store.Has("gameserver", ResourceSampler::CpuSeries));
}

TEST(ResourceSamplerTest, ARoundThatRanLateDoesNotReportASpikeThatNeverHappened)
{
    SeriesStore store;
    uint64 cpu = 0;
    ResourceSampler sampler(store, [&cpu](uint32) { return Used(cpu); });

    sampler.Sample({ Running("gameserver", 1234) }, 0);
    cpu = 1000000;
    std::vector<ResourceSampler::Reading> const late = sampler.Sample({ Running("gameserver", 1234) }, 10000);
    ASSERT_EQ(late.size(), 1u);
    EXPECT_NEAR(late[0].CpuPercent, 10.0, 0.001)
        << "the share is against the ten seconds that passed, not the two the round was meant to take";
}

TEST(ResourceSamplerTest, AnAppWithNoProcessLeavesAGapRatherThanZeroes)
{
    SeriesStore store;
    ResourceSampler sampler(store, [](uint32) { return Used(1000); });

    std::vector<ResourceSampler::Reading> const readings = sampler.Sample({ Stopped("gameserver") }, 0);
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_FALSE(readings[0].Sampled);
    EXPECT_FALSE(store.Has("gameserver", ResourceSampler::MemorySeries))
        << "a stopped app must draw a gap, and a zero would read as an app running and idle";
}

TEST(ResourceSamplerTest, AProcessTheSystemWillNotAnswerAboutIsSkipped)
{
    SeriesStore store;
    ResourceSampler sampler(store, [](uint32) { return std::optional<ProcessSnapshot>(); });

    std::vector<ResourceSampler::Reading> const readings = sampler.Sample({ Running("gameserver", 999999) }, 0);
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_FALSE(readings[0].Sampled);
    EXPECT_EQ(store.Count(), 0u);
}

TEST(ResourceSamplerTest, AnAppThatStoppedAndStartedAgainIsMeasuredAgainstItsNewProcess)
{
    SeriesStore store;
    uint64 cpu = 8000000;
    bool running = true;
    ResourceSampler sampler(store, [&cpu, &running](uint32) {
        return running ? std::optional<ProcessSnapshot>(Used(cpu)) : std::optional<ProcessSnapshot>();
    });

    sampler.Sample({ Running("gameserver", 1) }, 0);
    running = false;
    sampler.Sample({ Stopped("gameserver") }, 5000);

    running = true;
    cpu = 100000;
    std::vector<ResourceSampler::Reading> const back = sampler.Sample({ Running("gameserver", 2) }, 10000);
    ASSERT_EQ(back.size(), 1u);
    EXPECT_TRUE(back[0].Sampled);
    EXPECT_DOUBLE_EQ(back[0].CpuPercent, 0.0)
        << "the new process has its own total, and comparing it against the old one would read as a share below zero or a huge one";
}

TEST(ResourceSamplerTest, EveryAppIsWrittenUnderItsOwnName)
{
    SeriesStore store;
    ResourceSampler sampler(store, [](uint32 pid) { return Used(pid * 1000, pid * 2048); });

    sampler.Sample({ Running("loginserver", 10), Running("gameserver", 20), Running("patchserver", 30) }, 0);

    std::vector<std::string> const subjects = store.Subjects();
    EXPECT_EQ(subjects.size(), 3u);
    std::vector<SeriesPoint> const login = store.Between("loginserver", ResourceSampler::MemorySeries, 0, 60000, 100);
    std::vector<SeriesPoint> const game = store.Between("gameserver", ResourceSampler::MemorySeries, 0, 60000, 100);
    ASSERT_GT(PresentIn(login), 0u);
    ASSERT_GT(PresentIn(game), 0u);
    EXPECT_DOUBLE_EQ(login[0].Mean, 10.0 * 2048);
    EXPECT_DOUBLE_EQ(game[0].Mean, 20.0 * 2048) << "one app's memory must not be another's";
}

TEST(ResourceSamplerTest, ARoundIsCheapEnoughToSitOnATimerBesideTwentyApps)
{
    SeriesStore store;
    ResourceSampler sampler(store, [](uint32 pid) { return Used(pid * 1000); });

    std::vector<AppSnapshot> apps;
    for (int at = 0; at < 20; ++at)
        apps.push_back(Running("app" + std::to_string(at), at + 1));

    sampler.Sample(apps, 0);
    constexpr std::size_t Rounds = 200;
    std::vector<double> rounds;
    rounds.reserve(Rounds);
    for (std::size_t round = 1; round <= Rounds; ++round)
    {
        auto const started = std::chrono::steady_clock::now();
        sampler.Sample(apps, static_cast<int64>(round) * 5000);
        rounds.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count());
    }

    double const mean = std::accumulate(rounds.begin(), rounds.end(), 0.0) / static_cast<double>(Rounds);
    auto const middle = rounds.begin() + static_cast<std::ptrdiff_t>(Rounds / 2);
    std::nth_element(rounds.begin(), middle, rounds.end());
    double const perRound = *middle;
    double const perApp = perRound / static_cast<double>(apps.size());
    std::cout << "[ SAMPLER  ] " << perApp << " microseconds per app per sample, " << perRound
              << " microseconds for the median round of " << apps.size() << " apps, " << mean << " on average over " << Rounds
              << " rounds" << std::endl;
    EXPECT_LT(perRound, 1000.0) << "the median round of twenty apps costs " << perRound
                                << " microseconds, which is more than a millisecond of work per round";
}

TEST(ResourceSamplerTest, TheSeriesTheGraphDrawsMatchesAProcessUnderLoad)
{
    LogTestDirectory directory;
    std::filesystem::path const output = directory.Path() / "burn.txt";

    ChildLaunchOptions options;
    options.Program = std::filesystem::path(AMBROSE_CHILD_PROCESS_HELPER);
    options.Arguments = { "burn", "50", "7000" };
    options.OutputFile = output;

    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(options, error);
    ASSERT_TRUE(static_cast<bool>(child)) << error;

    AppSnapshot app;
    app.Name = "gameserver";
    app.ProcessId = child.GetIdentity().Id;
    app.State = AppState::Running;

    SeriesStore store;
    ResourceSampler sampler(store);

    auto const now = [] {
        return static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    };

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    sampler.Sample({ app }, now());
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));
    sampler.Sample({ app }, now());

    ASSERT_TRUE(child.WaitForExit(std::chrono::milliseconds(10000)));
    std::ifstream reading(output);
    std::string line;
    std::optional<double> held;
    while (std::getline(reading, line))
    {
        if (line.rfind("burnt ", 0) == 0)
            held = std::strtod(line.c_str() + 6, nullptr);
    }
    ASSERT_TRUE(held.has_value());

    std::vector<SeriesPoint> const points = store.Between("gameserver", ResourceSampler::CpuSeries, now() - 120000, now(), 200);
    double drawn = -1.0;
    for (auto at = points.rbegin(); at != points.rend(); ++at)
    {
        if (at->Present)
        {
            drawn = at->Mean;
            break;
        }
    }
    ASSERT_GE(drawn, 0.0) << "the processor series must hold a reading for the app that was running";
    std::cout << "[ GRAPHCPU ] the process held " << *held << "% of one core and the series the graph draws says " << drawn << "%"
              << std::endl;

    constexpr double Slack = 15.0;
    EXPECT_GT(drawn, *held - Slack) << "the graph would show " << drawn << "% where the process held " << *held << "%";
    EXPECT_LT(drawn, *held + Slack) << "the graph would show " << drawn << "% where the process held " << *held << "%";
}

TEST(ResourceSamplerTest, AStoppedAppLeavesAGapWhileTheOthersKeepBeingWritten)
{
    SeriesStore store;
    bool gameRunning = true;
    ResourceSampler sampler(store, [&gameRunning](uint32 pid) {
        if (pid == 20 && !gameRunning)
            return std::optional<ProcessSnapshot>();
        return std::optional<ProcessSnapshot>(Used(pid * 1000, pid * 4096));
    });

    std::vector<AppSnapshot> const both{ Running("loginserver", 10), Running("gameserver", 20) };
    std::vector<AppSnapshot> const oneGone{ Running("loginserver", 10), Stopped("gameserver") };

    sampler.Sample(both, 0);
    sampler.Sample(both, 5000);
    gameRunning = false;
    sampler.Sample(oneGone, 10000);
    sampler.Sample(oneGone, 15000);
    sampler.Sample(oneGone, 20000);

    std::vector<SeriesPoint> const game = store.Between("gameserver", ResourceSampler::MemorySeries, 0, 20000, 100);
    std::vector<SeriesPoint> const login = store.Between("loginserver", ResourceSampler::MemorySeries, 0, 20000, 100);

    ASSERT_EQ(game.size(), login.size());
    EXPECT_FALSE(game.back().Present) << "the app that stopped must leave a gap at the end of its graph";
    EXPECT_TRUE(login.back().Present) << "and the live view must go on updating for the app that did not";
    EXPECT_GT(PresentIn(login), PresentIn(game)) << "the running app has more readings than the stopped one";
}
