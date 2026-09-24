/*
 * Project Ambrose by Imjustchico
 * Checks what a process snapshot promises: this process reports resident memory and at least the thread asking, processor time only ever climbs and climbs when work is done, a share worked out from two readings is a share of the cores that exist rather than an unbounded number, a process that is not this one can be read, which is what the supervisor needs of the apps it runs, and a process that does not exist is refused rather than answered with zeroes, because a zero reads as an idle app and a refusal reads as a gap.
 */

#include "ChildProcess.h"
#include "ProcessInfo.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace
{
    using namespace Ambrose;

    std::filesystem::path HelperProgram()
    {
        return std::filesystem::path(AMBROSE_CHILD_PROCESS_HELPER);
    }

    uint64 NowMicroseconds()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}

TEST(ProcessInfoTest, ThisProcessReportsMemoryAndTheThreadsItHas)
{
    std::optional<ProcessSnapshot> const snapshot = ProcessInfo::Snapshot();
    ASSERT_TRUE(snapshot.has_value()) << "a process can always read itself";
    EXPECT_GT(snapshot->ResidentBytes, 0u) << "a running process holds some memory";
    EXPECT_GE(snapshot->ThreadCount, 1u) << "at least the thread asking the question";
    EXPECT_GT(ProcessInfo::CoreCount(), 0u);
}

TEST(ProcessInfoTest, ProcessorTimeClimbsWhenWorkIsDone)
{
    std::optional<ProcessSnapshot> const before = ProcessInfo::Snapshot();
    ASSERT_TRUE(before.has_value());

    uint64 const started = NowMicroseconds();
    volatile uint64 sum = 0;
    while (NowMicroseconds() - started < 120000)
        for (int spin = 0; spin < 20000; ++spin)
            sum = sum + static_cast<uint64>(spin);
    uint64 const elapsed = NowMicroseconds() - started;

    std::optional<ProcessSnapshot> const after = ProcessInfo::Snapshot();
    ASSERT_TRUE(after.has_value());
    EXPECT_GE(after->CpuMicroseconds, before->CpuMicroseconds) << "processor time is a total and never falls";
    EXPECT_GT(after->CpuMicroseconds, before->CpuMicroseconds) << "a tenth of a second of spinning must show as processor time";

    double const share = ProcessInfo::CpuShare(*before, *after, elapsed, ProcessInfo::CoreCount());
    EXPECT_GT(share, 0.0);
    EXPECT_LE(share, static_cast<double>(ProcessInfo::CoreCount()) * 100.0) << "no process uses more than every core it has";
}

TEST(ProcessInfoTest, AShareIsClampedRatherThanInventedWhenTheReadingsMakeNoSense)
{
    ProcessSnapshot earlier;
    ProcessSnapshot later;
    earlier.CpuMicroseconds = 1000;
    later.CpuMicroseconds = 500;
    EXPECT_EQ(ProcessInfo::CpuShare(earlier, later, 1000, 4), 0.0) << "a total that fell is a reading to throw away, not a negative share";

    later.CpuMicroseconds = 5000;
    EXPECT_EQ(ProcessInfo::CpuShare(earlier, later, 0, 4), 0.0) << "no time passed, so no share can be worked out";
    EXPECT_EQ(ProcessInfo::CpuShare(earlier, later, 1000, 1), 100.0) << "one core cannot do more than one core's worth";
    EXPECT_NEAR(ProcessInfo::CpuShare(earlier, later, 8000, 4), 50.0, 0.001);
}

TEST(ProcessInfoTest, AProcessThatIsNotThisOneCanBeRead)
{
    ChildLaunchOptions options;
    options.Program = HelperProgram();
    options.Arguments = { "sleep", "4000" };

    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(options, error);
    ASSERT_TRUE(static_cast<bool>(child)) << "the helper must be running to be read: " << error;
    uint32 const pid = static_cast<uint32>(child.GetIdentity().Id);
    EXPECT_NE(pid, ProcessInfo::CurrentProcessId()) << "the point of this test is a process that is not this one";

    std::optional<ProcessSnapshot> snapshot;
    for (int attempt = 0; attempt < 50 && !snapshot; ++attempt)
    {
        snapshot = ProcessInfo::SnapshotOf(pid);
        if (!snapshot)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(snapshot.has_value()) << "the supervisor reads the apps it runs the same way";
    EXPECT_GT(snapshot->ResidentBytes, 0u) << "another process holds memory too";
    EXPECT_GE(snapshot->ThreadCount, 1u);

    child.EndTree(error);
}

TEST(ProcessInfoTest, AProcessThatDoesNotExistIsRefusedRatherThanReadAsIdle)
{
    EXPECT_FALSE(ProcessInfo::SnapshotOf(0x7FFFFFFDu).has_value())
        << "an app that has gone must leave a gap in its graph, which needs a refusal rather than a row of zeroes";
}
