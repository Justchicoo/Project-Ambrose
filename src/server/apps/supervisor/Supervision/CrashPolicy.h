/*
 * Project Ambrose by Imjustchico
 * The supervisor's reading of why an app exited, how long to wait before starting it again, and what one crash leaves behind: the classification names the exit requested, clean but unexpected, a crash with its Windows exception or POSIX signal, out of memory, or a startup failure, the backoff grows exponentially from a second towards five minutes and resets after ten healthy minutes, a set number of crashes inside a window is a crash loop that stops restarting until someone starts the app by hand, and each crash keeps its time, exit code or signal, classification, uptime and the last 200 log lines, with no dump written until 17.83 lands.
 */

#ifndef AMBROSE_CRASHPOLICY_H
#define AMBROSE_CRASHPOLICY_H

#include "Types.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

enum class ExitClassification : uint8
{
    Requested,
    CleanUnexpected,
    Crash,
    OutOfMemory,
    StartupFailure
};

struct CrashRecord
{
    int64 EpochMs = 0;
    std::optional<int64> Code;
    std::optional<int> Signal;
    ExitClassification Classification = ExitClassification::Crash;
    int64 UptimeMs = 0;
    std::vector<std::string> LogTail;
    bool DumpWritten = false;
};

struct CrashEvent
{
    CrashRecord Record;
    bool CrashLoop = false;
    uint32 CrashesInWindow = 0;
};

struct CrashPolicySettings
{
    std::chrono::seconds BackoffBase{ 1 };
    std::chrono::seconds BackoffMax{ 300 };
    std::chrono::minutes HealthyReset{ 10 };
    uint32 CrashLoopCrashes = 4;
    std::chrono::minutes CrashLoopWindow{ 10 };

    static CrashPolicySettings Load(ConfigMgr const& config, std::vector<std::string>& problems);
};

std::string_view ClassificationName(ExitClassification classification) noexcept;
std::string_view SignalName(int signal) noexcept;
std::string_view ExceptionName(int64 code) noexcept;
ExitClassification ClassifyExit(bool requested, bool memoryKill, bool duringStart, std::optional<int64> code, std::optional<int> signal) noexcept;
std::string DescribeClassifiedExit(std::string_view name, ExitClassification classification, std::optional<int64> code, std::optional<int> signal, int64 uptimeMs);
bool HealthyResetDue(int64 readyEpochMs, int64 nowEpochMs, std::chrono::minutes healthyReset) noexcept;

class CrashPolicy
{
public:
    explicit CrashPolicy(CrashPolicySettings settings) : _settings(settings) {}

    std::chrono::seconds BackoffForCrash(uint32 consecutiveCrashes) const noexcept;
    bool IsCrashLoop(uint32 crashesInWindow) const noexcept { return crashesInWindow >= _settings.CrashLoopCrashes; }
    CrashPolicySettings const& Settings() const noexcept { return _settings; }

private:
    CrashPolicySettings _settings;
};

#endif
