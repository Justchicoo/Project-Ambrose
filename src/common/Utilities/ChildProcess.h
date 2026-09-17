/*
 * Project Ambrose by Imjustchico
 * Runs another program to completion: arguments passed exactly as given, no window, and no input unless InputEndsWithParent makes its input a pipe this process holds open without writing until Run returns, so that input ends only once Run is done or this process has ended in any way; every line it writes to standard output or error handed to a callback as UTF-8 and split past MaxLineBytes, and a timeout or stop request that ends it and everything it started, by force once TerminateGrace passes; whatever it started that still runs or holds its output open OutputDrainGrace after it exits is ended too; reports whether it started, its exit code or why none could be read, and whether it timed out or was stopped. ExitWhenInputEnds lets a program started that way end itself as soon as its input ends.
 */

#ifndef AMBROSE_CHILDPROCESS_H
#define AMBROSE_CHILDPROCESS_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ChildProcessOptions
{
    std::filesystem::path Program;
    std::vector<std::string> Arguments;
    std::filesystem::path WorkingDirectory;
    std::chrono::milliseconds Timeout{ 0 };
    std::function<void(std::string_view line, bool error)> OnLine;
    std::function<bool()> ShouldStop;
    bool InputEndsWithParent = false;
};

struct ChildProcessResult
{
    bool Started = false;
    std::optional<int> ExitCode;
    bool TimedOut = false;
    bool Stopped = false;
    std::string Error;

    bool Succeeded() const noexcept { return Started && ExitCode == 0 && !TimedOut && !Stopped; }
};

namespace ChildProcess
{
    inline constexpr std::chrono::milliseconds PollInterval{ 100 };
    inline constexpr std::chrono::milliseconds TerminateGrace{ 2000 };
    inline constexpr std::chrono::milliseconds OutputDrainGrace{ 2000 };
    inline constexpr std::size_t MaxLineBytes = 64 * 1024;

    ChildProcessResult Run(ChildProcessOptions const& options);
    std::string QuoteWindowsArgument(std::string_view argument);
    void ExitWhenInputEnds(int exitCode);
}

#endif
