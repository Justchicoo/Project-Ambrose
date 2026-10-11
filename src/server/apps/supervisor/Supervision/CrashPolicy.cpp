/*
 * Project Ambrose by Imjustchico
 * Reads why an app exited from its exit code, its signal and what it was doing, names the signal or the Windows exception a crash died with, works out the exponential wait before the next start, and decides when enough crashes inside the window make a crash loop, with the limits read from the supervisor's own options and every out-of-range value reported and clamped.
 */

#include "CrashPolicy.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    std::pair<std::string_view, std::string> HowItEnded(std::optional<int64> code, std::optional<int> signal)
    {
        if (signal)
        {
            std::string_view const name = SignalName(*signal);
            if (!name.empty())
                return { "crashed with", fmt::format("{} (signal {})", name, *signal) };
            return { "crashed with", fmt::format("signal {}", *signal) };
        }
        if (code)
        {
            std::string_view const name = ExceptionName(*code);
            if (!name.empty())
                return { "crashed with", fmt::format("exception {} (0x{:08X})", name, static_cast<uint64>(*code)) };
            if (*code >= 0xC0000000LL && *code <= 0xFFFFFFFFLL)
                return { "crashed with", fmt::format("exception 0x{:08X}", static_cast<uint64>(*code)) };
            return { "crashed with", fmt::format("exit code {}", *code) };
        }
        return { "crashed", "its exit could not be read because an earlier supervisor started it" };
    }
}

std::string_view ClassificationName(ExitClassification classification) noexcept
{
    switch (classification)
    {
        case ExitClassification::Requested: return "requested";
        case ExitClassification::CleanUnexpected: return "clean_unexpected";
        case ExitClassification::Crash: return "crash";
        case ExitClassification::OutOfMemory: return "out_of_memory";
        case ExitClassification::StartupFailure: return "startup_failure";
    }
    return "crash";
}

std::string_view SignalName(int signal) noexcept
{
    switch (signal)
    {
        case 1: return "SIGHUP";
        case 2: return "SIGINT";
        case 3: return "SIGQUIT";
        case 4: return "SIGILL";
        case 5: return "SIGTRAP";
        case 6: return "SIGABRT";
        case 7: return "SIGBUS";
        case 8: return "SIGFPE";
        case 9: return "SIGKILL";
        case 10: return "SIGUSR1";
        case 11: return "SIGSEGV";
        case 12: return "SIGUSR2";
        case 13: return "SIGPIPE";
        case 14: return "SIGALRM";
        case 15: return "SIGTERM";
        case 17: return "SIGCHLD";
        case 18: return "SIGCONT";
        case 19: return "SIGSTOP";
        case 20: return "SIGTSTP";
        case 21: return "SIGTTIN";
        case 22: return "SIGTTOU";
        case 23: return "SIGURG";
        case 24: return "SIGXCPU";
        case 25: return "SIGXFSZ";
        case 26: return "SIGVTALRM";
        case 27: return "SIGPROF";
        case 28: return "SIGWINCH";
        case 29: return "SIGIO";
        case 31: return "SIGSYS";
        default: break;
    }
    return {};
}

std::string_view ExceptionName(int64 code) noexcept
{
    switch (static_cast<uint32>(code))
    {
        case 0xC0000005: return "ACCESS_VIOLATION";
        case 0xC0000006: return "IN_PAGE_ERROR";
        case 0xC000001D: return "ILLEGAL_INSTRUCTION";
        case 0xC0000096: return "PRIVILEGED_INSTRUCTION";
        case 0xC00000FD: return "STACK_OVERFLOW";
        case 0xC0000135: return "DLL_NOT_FOUND";
        case 0xC0000142: return "DLL_INIT_FAILED";
        case 0xC0000374: return "HEAP_CORRUPTION";
        case 0xC0000409: return "STACK_BUFFER_OVERRUN";
        case 0x80000003: return "BREAKPOINT";
        case 0xE06D7363: return "CPP_EXCEPTION";
        default: break;
    }
    return {};
}

ExitClassification ClassifyExit(bool requested, bool memoryKill, bool duringStart, std::optional<int64> code, std::optional<int> signal) noexcept
{
    if (memoryKill)
        return ExitClassification::OutOfMemory;
    if (requested)
        return ExitClassification::Requested;
    if (duringStart)
        return ExitClassification::StartupFailure;
    if (signal)
        return ExitClassification::Crash;
    if (code && *code == 0)
        return ExitClassification::CleanUnexpected;
    return ExitClassification::Crash;
}

std::string DescribeClassifiedExit(std::string_view name, ExitClassification classification, std::optional<int64> code, std::optional<int> signal, int64 uptimeMs)
{
    if (classification == ExitClassification::Requested)
    {
        std::string how;
        if (signal)
        {
            std::string_view const named = SignalName(*signal);
            how = named.empty() ? fmt::format("was ended by signal {}", *signal) : fmt::format("was ended by {} (signal {})", named, *signal);
        }
        else if (code)
            how = fmt::format("exited with code {}", *code);
        else
            how = "exited, and its exit code could not be read because an earlier supervisor started it";
        return fmt::format("{} {} as asked after {} ms", name, how, uptimeMs);
    }
    auto const [verb, how] = HowItEnded(code, signal);
    switch (classification)
    {
        case ExitClassification::CleanUnexpected:
            return fmt::format("{} exited with code 0 without being asked after {} ms", name, uptimeMs);
        case ExitClassification::OutOfMemory:
            return fmt::format("{} was ended for exceeding its memory limit after {} ms", name, uptimeMs);
        case ExitClassification::StartupFailure:
            return fmt::format("{} failed during startup: {} {} after {} ms", name, verb, how, uptimeMs);
        case ExitClassification::Requested:
        case ExitClassification::Crash:
            break;
    }
    return fmt::format("{} {} {} without being asked after {} ms", name, verb, how, uptimeMs);
}

bool HealthyResetDue(int64 readyEpochMs, int64 nowEpochMs, std::chrono::minutes healthyReset) noexcept
{
    if (readyEpochMs == 0)
        return false;
    return nowEpochMs - readyEpochMs >= healthyReset.count() * 60000;
}

CrashPolicySettings CrashPolicySettings::Load(ConfigMgr const& config, std::vector<std::string>& problems)
{
    CrashPolicySettings settings;
    auto const bounded = [&problems](std::string_view key, int64 value, int64 low, int64 high)
    {
        int64 const clamped = std::clamp<int64>(value, low, high);
        if (clamped != value)
            problems.push_back(fmt::format("{} is {}, outside {} to {}; using {}", key, value, low, high, clamped));
        return clamped;
    };
    settings.BackoffBase = std::chrono::seconds(bounded("Supervisor.BackoffBaseSeconds", config.GetOption<int64>("Supervisor.BackoffBaseSeconds", 1, true), 0, 60));
    settings.BackoffMax = std::chrono::seconds(bounded("Supervisor.BackoffMaxSeconds", config.GetOption<int64>("Supervisor.BackoffMaxSeconds", 300, true), 1, 3600));
    settings.HealthyReset = std::chrono::minutes(bounded("Supervisor.HealthyResetMinutes", config.GetOption<int64>("Supervisor.HealthyResetMinutes", 10, true), 1, 1440));
    settings.CrashLoopCrashes = static_cast<uint32>(bounded("Supervisor.CrashLoopCrashes", config.GetOption<int64>("Supervisor.CrashLoopCrashes", 4, true), 2, 100));
    settings.CrashLoopWindow = std::chrono::minutes(bounded("Supervisor.CrashLoopWindowMinutes", config.GetOption<int64>("Supervisor.CrashLoopWindowMinutes", 10, true), 1, 1440));
    if (settings.BackoffBase > settings.BackoffMax)
    {
        problems.push_back(fmt::format("Supervisor.BackoffBaseSeconds is {} s, past Supervisor.BackoffMaxSeconds of {} s; using the maximum for both",
            settings.BackoffBase.count(), settings.BackoffMax.count()));
        settings.BackoffBase = settings.BackoffMax;
    }
    return settings;
}

std::chrono::seconds CrashPolicy::BackoffForCrash(uint32 consecutiveCrashes) const noexcept
{
    if (consecutiveCrashes == 0)
        return _settings.BackoffBase;
    uint64 wait = static_cast<uint64>(_settings.BackoffBase.count());
    for (uint32 doubled = 1; doubled < consecutiveCrashes; ++doubled)
    {
        wait *= 2;
        if (wait >= static_cast<uint64>(_settings.BackoffMax.count()))
            return _settings.BackoffMax;
    }
    return std::chrono::seconds(std::min<uint64>(wait, static_cast<uint64>(_settings.BackoffMax.count())));
}
