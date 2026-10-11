/*
 * Project Ambrose by Imjustchico
 * Tests the supervisor's reading of why an app exited: the classification from the exit code, the signal and what the app was doing, with the POSIX signal and the Windows exception each named, the exponential backoff from a second towards five minutes, the crash loop after enough crashes in the window, and the supervisor's own options for the limits with every out-of-range value reported and clamped.
 */

#include "ConfigMgr.h"
#include "CrashPolicy.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    CrashPolicySettings LoadSettings(std::string const& text, std::vector<std::string>& problems)
    {
        LogTestDirectory directory;
        std::filesystem::path const file = directory.Write("supervisor.conf", text);
        ConfigMgr config;
        EXPECT_TRUE(config.LoadInitial(file).Succeeded());
        return CrashPolicySettings::Load(config, problems);
    }
}

TEST(CrashPolicyTest, ClassificationNamesEveryClass)
{
    EXPECT_EQ(ClassificationName(ExitClassification::Requested), "requested");
    EXPECT_EQ(ClassificationName(ExitClassification::CleanUnexpected), "clean_unexpected");
    EXPECT_EQ(ClassificationName(ExitClassification::Crash), "crash");
    EXPECT_EQ(ClassificationName(ExitClassification::OutOfMemory), "out_of_memory");
    EXPECT_EQ(ClassificationName(ExitClassification::StartupFailure), "startup_failure");
}

TEST(CrashPolicyTest, SignalsAndExceptionsAreNamed)
{
    EXPECT_EQ(SignalName(9), "SIGKILL");
    EXPECT_EQ(SignalName(11), "SIGSEGV");
    EXPECT_EQ(SignalName(15), "SIGTERM");
    EXPECT_EQ(SignalName(2), "SIGINT");
    EXPECT_TRUE(SignalName(99).empty());
    EXPECT_EQ(ExceptionName(0xC0000005), "ACCESS_VIOLATION");
    EXPECT_EQ(ExceptionName(0xC00000FD), "STACK_OVERFLOW");
    EXPECT_EQ(ExceptionName(0xC0000409), "STACK_BUFFER_OVERRUN");
    EXPECT_TRUE(ExceptionName(0x1234).empty());
}

TEST(CrashPolicyTest, AnExitIsClassifiedFromWhatEndedItAndWhatItWasDoing)
{
    EXPECT_EQ(ClassifyExit(true, false, false, std::optional<int64>(0), std::nullopt), ExitClassification::Requested);
    EXPECT_EQ(ClassifyExit(true, false, false, std::nullopt, 9), ExitClassification::Requested);
    EXPECT_EQ(ClassifyExit(false, true, false, std::nullopt, 9), ExitClassification::OutOfMemory);
    EXPECT_EQ(ClassifyExit(false, false, true, std::optional<int64>(1), std::nullopt), ExitClassification::StartupFailure);
    EXPECT_EQ(ClassifyExit(false, false, true, std::nullopt, 11), ExitClassification::StartupFailure);
    EXPECT_EQ(ClassifyExit(false, false, false, std::nullopt, 11), ExitClassification::Crash);
    EXPECT_EQ(ClassifyExit(false, false, false, std::optional<int64>(0), std::nullopt), ExitClassification::CleanUnexpected);
    EXPECT_EQ(ClassifyExit(false, false, false, std::optional<int64>(1), std::nullopt), ExitClassification::Crash);
    EXPECT_EQ(ClassifyExit(false, false, false, std::optional<int64>(0xC0000005), std::nullopt), ExitClassification::Crash);
    EXPECT_EQ(ClassifyExit(false, false, false, std::nullopt, std::nullopt), ExitClassification::Crash);
}

TEST(CrashPolicyTest, TheDescriptionNamesTheSignalOrTheException)
{
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::Crash, std::nullopt, 11, 4500), "gameserver crashed with SIGSEGV (signal 11) without being asked after 4500 ms");
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::Crash, std::nullopt, 9, 100), "gameserver crashed with SIGKILL (signal 9) without being asked after 100 ms");
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::Crash, std::optional<int64>(0xC0000005), std::nullopt, 100),
        "gameserver crashed with exception ACCESS_VIOLATION (0xC0000005) without being asked after 100 ms");
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::CleanUnexpected, std::optional<int64>(0), std::nullopt, 100),
        "gameserver exited with code 0 without being asked after 100 ms");
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::OutOfMemory, std::nullopt, 9, 100), "gameserver was ended for exceeding its memory limit after 100 ms");
    EXPECT_EQ(DescribeClassifiedExit("gameserver", ExitClassification::Requested, std::optional<int64>(0), std::nullopt, 100), "gameserver exited with code 0 as asked after 100 ms");
}

TEST(CrashPolicyTest, BackoffGrowsFromASecondTowardsFiveMinutes)
{
    CrashPolicy const policy(CrashPolicySettings{});
    EXPECT_EQ(policy.BackoffForCrash(1), std::chrono::seconds(1));
    EXPECT_EQ(policy.BackoffForCrash(2), std::chrono::seconds(2));
    EXPECT_EQ(policy.BackoffForCrash(3), std::chrono::seconds(4));
    EXPECT_EQ(policy.BackoffForCrash(4), std::chrono::seconds(8));
    EXPECT_EQ(policy.BackoffForCrash(9), std::chrono::seconds(256));
    EXPECT_EQ(policy.BackoffForCrash(10), std::chrono::seconds(300));
    EXPECT_EQ(policy.BackoffForCrash(100), std::chrono::seconds(300));

    CrashPolicySettings custom;
    custom.BackoffBase = std::chrono::seconds(2);
    custom.BackoffMax = std::chrono::seconds(30);
    CrashPolicy const narrow(custom);
    EXPECT_EQ(narrow.BackoffForCrash(1), std::chrono::seconds(2));
    EXPECT_EQ(narrow.BackoffForCrash(4), std::chrono::seconds(16));
    EXPECT_EQ(narrow.BackoffForCrash(5), std::chrono::seconds(30));
    EXPECT_EQ(narrow.BackoffForCrash(6), std::chrono::seconds(30));
}

TEST(CrashPolicyTest, TheHealthyResetTripsAfterTenHealthyMinutes)
{
    EXPECT_FALSE(HealthyResetDue(0, 1000000, std::chrono::minutes(10)));
    EXPECT_FALSE(HealthyResetDue(1000000, 1000000 + 9 * 60000, std::chrono::minutes(10)));
    EXPECT_TRUE(HealthyResetDue(1000000, 1000000 + 10 * 60000, std::chrono::minutes(10)));
    EXPECT_TRUE(HealthyResetDue(1000000, 1000000 + 60 * 60000, std::chrono::minutes(10)));
}

TEST(CrashPolicyTest, TheCrashLoopTripsAtTheSetNumberOfCrashes)
{
    CrashPolicy const policy(CrashPolicySettings{});
    EXPECT_FALSE(policy.IsCrashLoop(0));
    EXPECT_FALSE(policy.IsCrashLoop(3));
    EXPECT_TRUE(policy.IsCrashLoop(4));
    EXPECT_TRUE(policy.IsCrashLoop(10));
}

TEST(CrashPolicyTest, TheLimitsComeFromTheSupervisorsOptionsWithDefaults)
{
    std::vector<std::string> problems;
    CrashPolicySettings const settings = LoadSettings("Supervisor.Apps = helper\n", problems);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(settings.BackoffBase, std::chrono::seconds(1));
    EXPECT_EQ(settings.BackoffMax, std::chrono::seconds(300));
    EXPECT_EQ(settings.HealthyReset, std::chrono::minutes(10));
    EXPECT_EQ(settings.CrashLoopCrashes, 4u);
    EXPECT_EQ(settings.CrashLoopWindow, std::chrono::minutes(10));
}

TEST(CrashPolicyTest, OutOfRangeLimitsAreReportedAndClamped)
{
    std::vector<std::string> problems;
    CrashPolicySettings const settings = LoadSettings("Supervisor.BackoffBaseSeconds = 600\nSupervisor.BackoffMaxSeconds = 0\nSupervisor.CrashLoopCrashes = 1\n", problems);
    EXPECT_EQ(settings.BackoffBase, std::chrono::seconds(1)) << "the base is clamped to 60 s and then to the maximum of 1 s";
    EXPECT_EQ(settings.BackoffMax, std::chrono::seconds(1));
    EXPECT_EQ(settings.CrashLoopCrashes, 2u);
    ASSERT_EQ(problems.size(), 4u) << "the base past its range, the maximum below its range, the crash count below its range, and the base past the maximum";
    EXPECT_NE(problems[0].find("Supervisor.BackoffBaseSeconds"), std::string::npos);
    EXPECT_NE(problems[1].find("Supervisor.BackoffMaxSeconds"), std::string::npos);
    EXPECT_NE(problems[2].find("Supervisor.CrashLoopCrashes"), std::string::npos);
    EXPECT_NE(problems[3].find("Supervisor.BackoffBaseSeconds"), std::string::npos);
}
