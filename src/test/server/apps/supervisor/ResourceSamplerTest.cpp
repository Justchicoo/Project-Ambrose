/*
 * Project Ambrose by Imjustchico
 * Checks what the sampler promises: the first reading of a process writes everything but the processor share, because a share needs two readings, the second works the share out against the time that actually passed rather than the time a round was meant to take, an app with no process is skipped so its graph carries a gap rather than zeroes, an app that stops and starts again is measured against its new process rather than against a total that belongs to the old one, every app is written under its own name, and a round costs few enough microseconds per app to sit on a timer beside twenty of them.
 */

#include "ResourceSampler.h"

#include <gtest/gtest.h>

#include <chrono>
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
    constexpr int Rounds = 200;
    auto const started = std::chrono::steady_clock::now();
    for (int round = 1; round <= Rounds; ++round)
        sampler.Sample(apps, round * 5000);
    auto const took = std::chrono::steady_clock::now() - started;

    double const microseconds = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(took).count());
    double const perRound = microseconds / Rounds;
    double const perApp = perRound / static_cast<double>(apps.size());
    std::cout << "[ SAMPLER  ] " << perApp << " microseconds per app per sample, " << perRound
              << " microseconds for a round of " << apps.size() << " apps" << std::endl;
    EXPECT_LT(perRound, 1000.0) << "a round of twenty apps costs " << perRound
                                << " microseconds, which is more than a millisecond of work per round";
}
