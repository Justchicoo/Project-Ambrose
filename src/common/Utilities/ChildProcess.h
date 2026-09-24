/*
 * Project Ambrose by Imjustchico
 * Runs another program to completion: arguments passed exactly as given, no window unless ShowsWindow says the program draws its own, and no input unless InputEndsWithParent makes its input a pipe this process holds open without writing until Run returns, so that input ends only once Run is done or this process has ended in any way; every line it writes to standard output or error handed to a callback as UTF-8 and split past MaxLineBytes, and a timeout or stop request that ends it and everything it started, by force once TerminateGrace passes; whatever it started that still runs or holds its output open OutputDrainGrace after it exits is ended too; reports whether it started, its exit code or why none could be read, and whether it timed out or was stopped. StartDetached instead starts a program that outlives this process, with no job object, no pipes and its own session, and waits for nothing, so its exit code is never read and this process reaps it only by ending. ExitWhenInputEnds lets a program started that way end itself as soon as its input ends. ChildProcessHandle.Launch starts a program meant to run for a long time and outlive this process: its output and errors appended to files, so they keep landing after this process ends, its input a pipe written without blocking or the null device, in a process group of its own, on Windows in a named job that does not end with this process and on POSIX in a session of its own; it keeps the program's identity, its process id, start time and executable and on Linux the boot it started in, so Adopt reopens it from a later process only while all of it still matches, and it waits for the exit, reads the exit code where the system gives it, interrupts the program's group with Ctrl+Break through a caller's helper on Windows or SIGTERM on POSIX, and ends the whole tree.
 */

#ifndef AMBROSE_CHILDPROCESS_H
#define AMBROSE_CHILDPROCESS_H

#include "Types.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
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
    bool ShowsWindow = false;
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

struct ChildProcessIdentity
{
    int64 Id = 0;
    uint64 StartTime = 0;
    std::string BootId;
    std::filesystem::path Executable;

    bool Matches(ChildProcessIdentity const& other) const;
};

using ChildBreakSender = std::function<bool(int64 group, std::string& error)>;

struct ChildLaunchOptions
{
    std::filesystem::path Program;
    std::vector<std::string> Arguments;
    std::filesystem::path WorkingDirectory;
    std::filesystem::path OutputFile;
    std::filesystem::path ErrorFile;
    bool KeepInput = false;
    ChildBreakSender SendBreak;
};

struct ChildExit
{
    std::optional<int64> Code;
    std::optional<int> Signal;
};

class ChildProcessHandle
{
public:
    ChildProcessHandle() noexcept;
    ~ChildProcessHandle();
    ChildProcessHandle(ChildProcessHandle&& other) noexcept;
    ChildProcessHandle& operator=(ChildProcessHandle&& other) noexcept;
    ChildProcessHandle(ChildProcessHandle const&) = delete;
    ChildProcessHandle& operator=(ChildProcessHandle const&) = delete;

    static ChildProcessHandle Launch(ChildLaunchOptions const& options, std::string& error);
    static ChildProcessHandle Adopt(ChildProcessIdentity const& identity, ChildBreakSender sendBreak, std::string& error);
    static std::optional<ChildProcessIdentity> Describe(int64 id);

    explicit operator bool() const noexcept { return static_cast<bool>(_state); }
    ChildProcessIdentity const& GetIdentity() const;
    bool IsAdopted() const;
    bool HasInput() const;
    bool WaitForExit(std::chrono::milliseconds timeout);
    std::optional<ChildExit> GetExit() const;
    bool WriteInput(std::string_view text, std::string& error);
    bool Interrupt(std::string& error);
    bool EndTree(std::string& error);

    struct State;

private:
    explicit ChildProcessHandle(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> _state;
};

namespace ChildProcess
{
    inline constexpr std::chrono::milliseconds PollInterval{ 100 };
    inline constexpr std::chrono::milliseconds TerminateGrace{ 2000 };
    inline constexpr std::chrono::milliseconds OutputDrainGrace{ 2000 };
    inline constexpr std::size_t MaxLineBytes = 64 * 1024;

    ChildProcessResult Run(ChildProcessOptions const& options);
    ChildProcessResult StartDetached(ChildProcessOptions const& options);
    std::string QuoteWindowsArgument(std::string_view argument);
    void ExitWhenInputEnds(int exitCode);
    bool SendConsoleBreak(int64 group, std::string& error);
}

#endif
