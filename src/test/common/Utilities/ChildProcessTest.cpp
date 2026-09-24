/*
 * Project Ambrose by Imjustchico
 * Tests ChildProcess by running the child_process_helper program: exit codes and the lines written to standard output and error, arguments with spaces, quotes, backslashes, empty strings and UTF-8 arriving exactly, a program path holding spaces and UTF-8 and, on Windows, named without .exe, lines split at MaxLineBytes without cutting a character, CRLF, a last line with no newline, invalid UTF-8 replaced, many lines in order, closed input and the working directory, an input that ends with the parent staying open while the child runs, so a child watching it keeps running, and ending, once Run returns, a copy of the helper in a process group of its own that exits when that input ends, a child watching an input already at its end exiting at once with the code it chose, programs and arguments that cannot start, a timeout, a stop request, a child ignoring SIGTERM and a grandchild holding the output open each ending the child and its grandchild, and on POSIX the exit code still read when SIGCHLD is ignored or set to reap children itself; StartDetached runs a program with no pipes and no exit code of its own, which a file that program writes shows, and reports a program that cannot start; also checks QuoteWindowsArgument against CommandLineToArgvW. ChildProcessHandle tests launch the helper with its output and errors in files it keeps appending to after the launcher lets go, stop a copy that waits like a server with a shutdown line on its input and with an interrupt, which is Ctrl+Break through the helper itself on Windows and SIGTERM on POSIX, end a child and its grandchild as one tree, adopt a running child from its identity after the launching handle is gone and refuse the same process id with another start time or executable, find no identity for a process that has ended, and report a program that cannot start.
 */

#include "ChildProcess.h"
#include "LogTestDirectory.h"
#include "ScopeExit.h"
#include "StringUtil.h"
#include "Utf.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#endif

namespace
{
    using namespace std::chrono_literals;

    struct Captured
    {
        ChildProcessResult Result;
        std::vector<std::string> Output;
        std::vector<std::string> Errors;
        std::chrono::milliseconds Elapsed{ 0 };
    };

    std::filesystem::path Utf8Path(std::string_view text)
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    std::filesystem::path HelperPath()
    {
        return Utf8Path(AMBROSE_CHILD_PROCESS_HELPER);
    }

    void CopyHelperTo(std::filesystem::path const& folder)
    {
        std::filesystem::copy_file(HelperPath(), folder / HelperPath().filename());
        std::string_view dlls = AMBROSE_CHILD_PROCESS_HELPER_DLLS;
        while (!dlls.empty())
        {
            std::size_t const end = dlls.find('|');
            std::filesystem::path const dll = Utf8Path(std::string(dlls.substr(0, end)));
            std::filesystem::copy_file(dll, folder / dll.filename(), std::filesystem::copy_options::skip_existing);
            dlls = end == std::string_view::npos ? std::string_view() : dlls.substr(end + 1);
        }
    }

    ChildProcessOptions HelperOptions(std::vector<std::string> arguments)
    {
        ChildProcessOptions options;
        options.Program = HelperPath();
        options.Arguments = std::move(arguments);
        options.Timeout = 60s;
        return options;
    }

    Captured Capture(ChildProcessOptions options)
    {
        Captured captured;
        options.OnLine = [&captured](std::string_view line, bool error)
        {
            (error ? captured.Errors : captured.Output).emplace_back(line);
        };
        std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
        captured.Result = ChildProcess::Run(options);
        captured.Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        return captured;
    }

    std::vector<std::string> ExactArguments()
    {
        return {
            "",
            "plain",
            "with space",
            "  leading and trailing  ",
            "quote\"inside",
            "\"",
            "\"\"",
            "back\\slash",
            "trailing\\",
            "trailing space \\",
            "\\\\server\\share\\",
            "\\\"",
            "\\\\\"",
            "tab\there",
            "caf\xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x8E\xA9",
            "*?<>|&^%PATH%",
            ""
        };
    }

    bool ProcessEnded(long long id)
    {
        std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + 15s;
        while (std::chrono::steady_clock::now() < until)
        {
#ifdef _WIN32
            HANDLE const process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(id));
            if (process == nullptr)
                return true;
            DWORD const wait = WaitForSingleObject(process, 100);
            CloseHandle(process);
            if (wait == WAIT_OBJECT_0)
                return true;
#else
            if (kill(static_cast<pid_t>(id), 0) != 0 && errno == ESRCH)
                return true;
            std::ifstream statFile("/proc/" + std::to_string(id) + "/stat");
            std::string text;
            std::getline(statFile, text);
            std::size_t const nameEnd = text.rfind(')');
            if (nameEnd != std::string::npos && nameEnd + 2 < text.size() && (text[nameEnd + 2] == 'Z' || text[nameEnd + 2] == 'X'))
                return true;
            std::this_thread::sleep_for(100ms);
#endif
        }
        return false;
    }

    void ExpectGrandchildEnded(std::vector<std::string> const& output)
    {
        ASSERT_FALSE(output.empty());
        std::optional<long long> const grandchild = Ambrose::StringTo<long long>(output.front());
        ASSERT_TRUE(grandchild) << output.front();
        EXPECT_TRUE(ProcessEnded(*grandchild)) << "grandchild " << *grandchild << " is still running";
    }

    std::string ReadWhole(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> Lines(std::filesystem::path const& file)
    {
        std::string const text = ReadWhole(file);
        std::vector<std::string> lines;
        for (std::string_view const line : Ambrose::Tokenize(text, '\n', false))
            lines.emplace_back(line);
        return lines;
    }

    bool WaitForText(std::filesystem::path const& file, std::string_view expected)
    {
        std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + 30s;
        while (std::chrono::steady_clock::now() < until)
        {
            if (ReadWhole(file).find(expected) != std::string::npos)
                return true;
            std::this_thread::sleep_for(20ms);
        }
        return false;
    }

    ChildLaunchOptions LaunchOptions(LogTestDirectory const& directory, std::vector<std::string> arguments)
    {
        ChildLaunchOptions options;
        options.Program = HelperPath();
        options.Arguments = std::move(arguments);
        options.OutputFile = directory.Path() / "out.log";
        options.ErrorFile = directory.Path() / "err.log";
        options.SendBreak = [](int64 group, std::string& error)
        {
            ChildProcessResult const result = ChildProcess::Run(HelperOptions({ "console-break", std::to_string(group) }));
            if (result.Succeeded())
                return true;
            error = result.Error.empty() ? "the helper could not send Ctrl+Break" : result.Error;
            return false;
        };
        return options;
    }

    bool SameExecutable(std::filesystem::path const& left, std::filesystem::path const& right)
    {
        ChildProcessIdentity first;
        first.Executable = left;
        ChildProcessIdentity second;
        second.Executable = right;
        return first.Matches(second);
    }

    void ExpectNotStarted(ChildProcessOptions const& options, std::string_view expected)
    {
        Captured const captured = Capture(options);
        EXPECT_FALSE(captured.Result.Started);
        EXPECT_FALSE(captured.Result.ExitCode);
        EXPECT_FALSE(captured.Result.Succeeded());
        EXPECT_TRUE(captured.Output.empty());
        EXPECT_TRUE(Utf::IsValidUtf8(captured.Result.Error));
        EXPECT_NE(captured.Result.Error.find(expected), std::string::npos) << captured.Result.Error;
    }
}

TEST(ChildProcessTest, QuoteWindowsArgumentFollowsCommandLineToArgvWRules)
{
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument(""), "\"\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("plain"), "plain");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("C:\\dir\\file"), "C:\\dir\\file");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("with space"), "\"with space\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("say \"hi\""), "\"say \\\"hi\\\"\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("a\\\"b"), "\"a\\\\\\\"b\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("trailing \\"), "\"trailing \\\\\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("tab\there"), "\"tab\there\"");
    EXPECT_EQ(ChildProcess::QuoteWindowsArgument("line\nbreak"), "\"line\nbreak\"");
#ifdef _WIN32
    std::vector<std::string> samples = ExactArguments();
    samples.push_back("line\nbreak");
    samples.push_back("\\\\\\\\");
    std::string commandLine = "program";
    for (std::string const& sample : samples)
        commandLine.append(" ").append(ChildProcess::QuoteWindowsArgument(sample));
    std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(commandLine, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(wide);
    int count = 0;
    LPWSTR* const parsed = CommandLineToArgvW(reinterpret_cast<wchar_t const*>(wide->c_str()), &count);
    ASSERT_NE(parsed, nullptr);
    ASSERT_EQ(static_cast<std::size_t>(count), samples.size() + 1);
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        std::optional<std::string> const argument = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(parsed[index + 1])), Utf::InvalidPolicy::Reject);
        EXPECT_EQ(argument, samples[index]) << "argument " << index;
    }
    LocalFree(parsed);
#endif
}

TEST(ChildProcessTest, ReportsTheExitCodeAndEachLineOfOutputAndErrors)
{
    Captured const failed = Capture(HelperOptions({ "echo", "first line", "err", "problem one", "echo", "second line", "err", "problem two", "exit", "7" }));
    ASSERT_TRUE(failed.Result.Started) << failed.Result.Error;
    EXPECT_EQ(failed.Result.ExitCode, 7);
    EXPECT_FALSE(failed.Result.TimedOut);
    EXPECT_FALSE(failed.Result.Stopped);
    EXPECT_FALSE(failed.Result.Succeeded());
    EXPECT_TRUE(failed.Result.Error.empty()) << failed.Result.Error;
    EXPECT_EQ(failed.Output, (std::vector<std::string>{ "first line", "second line" }));
    EXPECT_EQ(failed.Errors, (std::vector<std::string>{ "problem one", "problem two" }));

    Captured const succeeded = Capture(HelperOptions({ "echo", "done" }));
    ASSERT_TRUE(succeeded.Result.Started) << succeeded.Result.Error;
    EXPECT_EQ(succeeded.Result.ExitCode, 0);
    EXPECT_TRUE(succeeded.Result.Succeeded());
    EXPECT_EQ(succeeded.Output, (std::vector<std::string>{ "done" }));
    EXPECT_TRUE(succeeded.Errors.empty());
}

TEST(ChildProcessTest, PassesArgumentsExactly)
{
    std::vector<std::string> const expected = ExactArguments();
    std::vector<std::string> arguments{ "args" };
    arguments.insert(arguments.end(), expected.begin(), expected.end());
    Captured const captured = Capture(HelperOptions(arguments));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    EXPECT_EQ(captured.Output, expected);
    EXPECT_TRUE(captured.Errors.empty());
}

TEST(ChildProcessTest, RunsAProgramWhosePathHoldsSpacesAndUtf8)
{
    LogTestDirectory directory;
    std::filesystem::path const folder = directory.Path() / Utf8Path("child helper \xC3\xA9 \xE2\x9C\x93");
    std::filesystem::create_directories(folder);
    std::filesystem::path const copy = folder / HelperPath().filename();
    CopyHelperTo(folder);
#ifndef _WIN32
    std::filesystem::permissions(copy, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#endif
    ChildProcessOptions options = HelperOptions({ "echo", "moved", "args", "last" });
    options.Program = copy;
    Captured const captured = Capture(options);
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    EXPECT_EQ(captured.Output, (std::vector<std::string>{ "moved", "last" }));
#ifdef _WIN32
    ASSERT_EQ(copy.extension().string(), ".exe");
    options.Program = folder / copy.stem();
    Captured const withoutExtension = Capture(options);
    ASSERT_TRUE(withoutExtension.Result.Started) << withoutExtension.Result.Error;
    EXPECT_TRUE(withoutExtension.Result.Succeeded());
    EXPECT_EQ(withoutExtension.Output, (std::vector<std::string>{ "moved", "last" }));
#endif
}

TEST(ChildProcessTest, SplitsLinesLongerThanMaxLineBytesWithoutCuttingACharacter)
{
    std::size_t const limit = ChildProcess::MaxLineBytes;
    Captured const captured = Capture(HelperOptions({ "long", std::to_string(limit * 2 + 5), "long", std::to_string(limit), "long", "0", "pad", std::to_string(limit - 1), "\xC3\xA9", "echo", "after" }));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    ASSERT_EQ(captured.Output.size(), 8u);
    std::vector<std::size_t> const sizes{ limit, limit, 5, limit, 0, limit - 1, 2, 5 };
    for (std::size_t index = 0; index < sizes.size(); ++index)
        EXPECT_EQ(captured.Output[index].size(), sizes[index]) << "line " << index;
    for (std::size_t index = 0; index < 6; ++index)
        EXPECT_EQ(captured.Output[index].find_first_not_of('x'), std::string::npos) << "line " << index;
    EXPECT_EQ(captured.Output[6], "\xC3\xA9");
    EXPECT_EQ(captured.Output[7], "after");
}

TEST(ChildProcessTest, DeliversPiecesOfALongLineBeforeItEnds)
{
    std::size_t const limit = ChildProcess::MaxLineBytes;
    std::vector<std::string> output;
    ChildProcessOptions options = HelperOptions({ "unfinished", std::to_string(limit * 2 + 10), "sleep", "60000" });
    options.OnLine = [&output](std::string_view line, bool)
    {
        output.emplace_back(line);
    };
    options.ShouldStop = [&output]
    {
        return output.size() >= 2;
    };
    ChildProcessResult const result = ChildProcess::Run(options);
    ASSERT_TRUE(result.Started) << result.Error;
    EXPECT_TRUE(result.Stopped);
    EXPECT_FALSE(result.TimedOut);
    ASSERT_EQ(output.size(), 3u);
    EXPECT_EQ(output[0].size(), limit);
    EXPECT_EQ(output[1].size(), limit);
    EXPECT_EQ(output[2], std::string(10, 'x'));
}

TEST(ChildProcessTest, EndsLinesAtCrlfAndAtTheEndOfOutputAndReplacesInvalidUtf8)
{
    Captured const captured = Capture(HelperOptions({ "crlf", "windows line", "hex", "41ff42", "hex", "610062", "echo", "caf\xC3\xA9", "partial", "no newline" }));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    std::vector<std::string> const expected{ "windows line", "A\xEF\xBF\xBD" "B", std::string("a\0b", 3), "caf\xC3\xA9", "no newline" };
    EXPECT_EQ(captured.Output, expected);
}

TEST(ChildProcessTest, DeliversManyLinesInOrder)
{
    std::size_t const count = 50000;
    Captured const captured = Capture(HelperOptions({ "lines", std::to_string(count), "err", "end" }));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    ASSERT_EQ(captured.Output.size(), count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (captured.Output[index] != std::to_string(index))
        {
            ADD_FAILURE() << "line " << index << " is " << captured.Output[index];
            break;
        }
    }
    EXPECT_EQ(captured.Errors, (std::vector<std::string>{ "end" }));
}

TEST(ChildProcessTest, GivesTheChildNoInputAndItsWorkingDirectory)
{
    LogTestDirectory directory;
    std::filesystem::path const folder = directory.Path() / Utf8Path("work \xC3\xA9");
    std::filesystem::create_directories(folder);
    ChildProcessOptions options = HelperOptions({ "stdin", "input", "cwd" });
    options.WorkingDirectory = folder;
    Captured const captured = Capture(options);
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded());
    ASSERT_EQ(captured.Output.size(), 3u);
    EXPECT_EQ(captured.Output[0], "eof");
    EXPECT_EQ(captured.Output[1], "eof");
    std::error_code error;
    EXPECT_TRUE(std::filesystem::equivalent(Utf8Path(captured.Output[2]), folder, error)) << captured.Output[2];
}

TEST(ChildProcessTest, AnInputThatEndsWithTheParentStaysOpenWhileTheChildRunsAndEndsAChildWatchingIt)
{
    ChildProcessOptions options = HelperOptions({ "input", "spawn-input-watcher", "exit-when-input-ends", "9", "sleep", "300", "echo", "alive" });
    options.InputEndsWithParent = true;
    Captured const captured = Capture(options);
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.Succeeded()) << captured.Result.Error;
    EXPECT_TRUE(captured.Errors.empty());
    EXPECT_LT(captured.Elapsed, ChildProcess::OutputDrainGrace);
    ASSERT_EQ(captured.Output.size(), 3u);
    EXPECT_EQ(captured.Output[0], "open");
    EXPECT_EQ(captured.Output[2], "alive");
    ExpectGrandchildEnded(std::vector<std::string>(captured.Output.begin() + 1, captured.Output.begin() + 2));
}

TEST(ChildProcessTest, AChildWatchingItsInputExitsWithItsCodeOnceThatInputEnds)
{
    Captured const captured = Capture(HelperOptions({ "exit-when-input-ends", "9", "sleep", "60000", "echo", "never" }));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_EQ(captured.Result.ExitCode, 9) << captured.Result.Error;
    EXPECT_FALSE(captured.Result.TimedOut);
    EXPECT_TRUE(captured.Output.empty());
    EXPECT_LT(captured.Elapsed, 15s);
}

#ifndef _WIN32
TEST(ChildProcessTest, ReadsTheExitCodeWhenSigchldWouldReapTheChild)
{
    struct sigaction previous{};
    ASSERT_EQ(sigaction(SIGCHLD, nullptr, &previous), 0);
    ScopeExit const restore([&previous] { sigaction(SIGCHLD, &previous, nullptr); });
    for (bool const ignored : { true, false })
    {
        struct sigaction reaping{};
        reaping.sa_handler = ignored ? SIG_IGN : SIG_DFL;
        reaping.sa_flags = ignored ? 0 : SA_NOCLDWAIT;
        sigemptyset(&reaping.sa_mask);
        ASSERT_EQ(sigaction(SIGCHLD, &reaping, nullptr), 0);
        Captured const captured = Capture(HelperOptions({ "echo", "done", "exit", "5" }));
        ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
        EXPECT_EQ(captured.Result.ExitCode, 5) << captured.Result.Error;
        EXPECT_TRUE(captured.Result.Error.empty()) << captured.Result.Error;
        EXPECT_EQ(captured.Output, (std::vector<std::string>{ "done" }));
        struct sigaction after{};
        ASSERT_EQ(sigaction(SIGCHLD, nullptr, &after), 0);
        EXPECT_EQ(after.sa_flags & SA_NOCLDWAIT, 0);
        EXPECT_TRUE(after.sa_handler != SIG_IGN);
    }
}
#endif

TEST(ChildProcessTest, ReportsProgramsAndArgumentsThatCannotStart)
{
    LogTestDirectory directory;

    ChildProcessOptions missingPath = HelperOptions({ "echo", "never" });
    missingPath.Program = directory.Path() / "missing-program.exe";
    ExpectNotStarted(missingPath, "could not be started");

    ChildProcessOptions missingName = HelperOptions({ "echo", "never" });
    missingName.Program = "ambrose-child-process-missing-program";
    ExpectNotStarted(missingName, "could not be started");

    ChildProcessOptions unnamed = HelperOptions({ "echo", "never" });
    unnamed.Program.clear();
    ExpectNotStarted(unnamed, "no program was named");

    ExpectNotStarted(HelperOptions({ "echo", std::string("a\0b", 3) }), "holds a NUL character");

    ChildProcessOptions missingFolder = HelperOptions({ "echo", "never" });
    missingFolder.WorkingDirectory = directory.Path() / "missing-folder";
    ExpectNotStarted(missingFolder, "could not be started");

#ifdef _WIN32
    ExpectNotStarted(HelperOptions({ "echo", "bad \xFF byte" }), "is not valid UTF-8");

    ChildProcessOptions batch = HelperOptions({ "echo", "never" });
    batch.Program = directory.Write("script.CMD", "@echo off\r\n");
    ExpectNotStarted(batch, "batch file");

    ExpectNotStarted(HelperOptions({ "args", std::string(40000, 'x') }), "Windows allows at most");
#endif
}

TEST(ChildProcessTest, TimeoutEndsTheChildAndItsGrandchild)
{
    ChildProcessOptions options = HelperOptions({ "spawn-sleeper", "sleep", "60000" });
    options.Timeout = 2s;
    Captured const captured = Capture(options);
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_TRUE(captured.Result.TimedOut);
    EXPECT_FALSE(captured.Result.Stopped);
    EXPECT_FALSE(captured.Result.ExitCode);
    EXPECT_FALSE(captured.Result.Succeeded());
    EXPECT_GE(captured.Elapsed, options.Timeout);
    EXPECT_LT(captured.Elapsed, options.Timeout + ChildProcess::TerminateGrace + 15s);
    EXPECT_EQ(captured.Output.size(), 1u);
    ExpectGrandchildEnded(captured.Output);
}

TEST(ChildProcessTest, StopRequestEndsTheChildAndItsGrandchildWithoutATimeout)
{
    std::vector<std::string> output;
    int checks = 0;
    ChildProcessOptions options = HelperOptions({ "spawn-sleeper", "sleep", "60000" });
    options.Timeout = 0ms;
    options.OnLine = [&output](std::string_view line, bool)
    {
        output.emplace_back(line);
    };
    options.ShouldStop = [&output, &checks]
    {
        ++checks;
        return !output.empty();
    };
    std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
    ChildProcessResult const result = ChildProcess::Run(options);
    std::chrono::steady_clock::duration const elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(result.Started) << result.Error;
    EXPECT_TRUE(result.Stopped);
    EXPECT_FALSE(result.TimedOut);
    EXPECT_FALSE(result.ExitCode);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_GT(checks, 0);
    EXPECT_LT(elapsed, ChildProcess::TerminateGrace + 15s);
    EXPECT_EQ(output.size(), 1u);
    ExpectGrandchildEnded(output);
}

TEST(ChildProcessTest, ChildIgnoringTerminationIsKilledAfterTheGrace)
{
    bool ready = false;
    ChildProcessOptions options = HelperOptions({ "ignore-term", "echo", "ready", "sleep", "60000" });
    options.OnLine = [&ready](std::string_view line, bool)
    {
        ready = ready || line == "ready";
    };
    options.ShouldStop = [&ready]
    {
        return ready;
    };
    std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
    ChildProcessResult const result = ChildProcess::Run(options);
    std::chrono::steady_clock::duration const elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(result.Started) << result.Error;
    EXPECT_TRUE(ready);
    EXPECT_TRUE(result.Stopped);
    EXPECT_FALSE(result.ExitCode);
    EXPECT_LT(elapsed, ChildProcess::TerminateGrace + 15s);
#ifndef _WIN32
    EXPECT_GE(elapsed, ChildProcess::TerminateGrace);
#endif
}

TEST(ChildProcessTest, ReturnsWhenTheChildExitsWhileItsGrandchildHoldsTheOutputOpen)
{
    Captured const captured = Capture(HelperOptions({ "spawn-sleeper", "exit", "3" }));
    ASSERT_TRUE(captured.Result.Started) << captured.Result.Error;
    EXPECT_EQ(captured.Result.ExitCode, 3);
    EXPECT_FALSE(captured.Result.TimedOut);
    EXPECT_FALSE(captured.Result.Stopped);
    EXPECT_TRUE(captured.Result.Error.empty()) << captured.Result.Error;
    EXPECT_GE(captured.Elapsed, ChildProcess::OutputDrainGrace);
    EXPECT_LT(captured.Elapsed, ChildProcess::OutputDrainGrace + 15s);
    EXPECT_EQ(captured.Output.size(), 1u);
    ExpectGrandchildEnded(captured.Output);
}

TEST(ChildProcessTest, StartDetachedRunsAProgramWithoutWaitingForIt)
{
    LogTestDirectory directory;
    std::filesystem::path const marker = directory.Path() / "started.txt";
    std::u8string const path = marker.u8string();
    ChildProcessOptions options;
    options.Program = HelperPath();
    options.Arguments = { "touch", std::string(path.begin(), path.end()) };
    ChildProcessResult const result = ChildProcess::StartDetached(options);
    ASSERT_TRUE(result.Started) << result.Error;
    EXPECT_FALSE(result.ExitCode);
    EXPECT_TRUE(result.Error.empty()) << result.Error;
    std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + 15s;
    while (!std::filesystem::exists(marker) && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(std::filesystem::exists(marker));
}

TEST(ChildProcessTest, StartDetachedReportsAProgramThatCannotStart)
{
    ChildProcessOptions options;
    options.Program = HelperPath().parent_path() / "no-such-program-here";
    ChildProcessResult const result = ChildProcess::StartDetached(options);
    EXPECT_FALSE(result.Started);
    EXPECT_NE(result.Error.find("no-such-program-here"), std::string::npos) << result.Error;
    options.Program.clear();
    ChildProcessResult const nothing = ChildProcess::StartDetached(options);
    EXPECT_FALSE(nothing.Started);
    EXPECT_EQ(nothing.Error, "no program was named to run");
}

TEST(ChildProcessHandleTest, LaunchWritesOutputAndErrorsToFilesAndReportsTheExitCode)
{
    LogTestDirectory directory;
    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(LaunchOptions(directory, { "echo", "out", "err", "bad", "exit", "3" }), error);
    ASSERT_TRUE(child) << error;
    EXPECT_GT(child.GetIdentity().Id, 0);
    EXPECT_TRUE(SameExecutable(child.GetIdentity().Executable.filename(), HelperPath().filename())) << child.GetIdentity().Executable;
    EXPECT_FALSE(child.IsAdopted());
    EXPECT_FALSE(child.HasInput());
    ASSERT_TRUE(child.WaitForExit(30s));
    std::optional<ChildExit> const exit = child.GetExit();
    ASSERT_TRUE(exit.has_value());
    EXPECT_EQ(exit->Code, std::optional<int64>(3));
    EXPECT_EQ(ReadWhole(directory.Path() / "out.log"), "out\n");
    EXPECT_EQ(ReadWhole(directory.Path() / "err.log"), "bad\n");
    EXPECT_FALSE(ChildProcessHandle::Describe(child.GetIdentity().Id).has_value());
}

TEST(ChildProcessHandleTest, OutputKeepsLandingAfterTheLaunchingHandleIsGone)
{
    LogTestDirectory directory;
    std::string error;
    {
        ChildProcessHandle child = ChildProcessHandle::Launch(LaunchOptions(directory, { "sleep", "300", "echo", "later" }), error);
        ASSERT_TRUE(child) << error;
    }
    EXPECT_TRUE(WaitForText(directory.Path() / "out.log", "later\n"));
}

TEST(ChildProcessHandleTest, AShutdownLineOnItsInputStopsAChildThatWaitsLikeAServer)
{
    LogTestDirectory directory;
    ChildLaunchOptions options = LaunchOptions(directory, { "wait-for-stop" });
    options.KeepInput = true;
    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(options, error);
    ASSERT_TRUE(child) << error;
    ScopeExit const cleanup([&child] { std::string ignored; child.EndTree(ignored); });
    EXPECT_TRUE(child.HasInput());
    ASSERT_TRUE(WaitForText(directory.Path() / "out.log", "waiting\n"));
    ASSERT_TRUE(child.WriteInput("shutdown\n", error)) << error;
    ASSERT_TRUE(child.WaitForExit(30s));
    EXPECT_EQ(child.GetExit()->Code, std::optional<int64>(0));
    EXPECT_NE(ReadWhole(directory.Path() / "out.log").find("stopped by shutdown"), std::string::npos);
    EXPECT_FALSE(child.WriteInput("shutdown\n", error));
}

TEST(ChildProcessHandleTest, AnInterruptStopsTheChildsGroupTheWayASignalStopsAServer)
{
    LogTestDirectory directory;
    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(LaunchOptions(directory, { "wait-for-stop" }), error);
    ASSERT_TRUE(child) << error;
    ScopeExit const cleanup([&child] { std::string ignored; child.EndTree(ignored); });
    ASSERT_TRUE(WaitForText(directory.Path() / "out.log", "waiting\n"));
    ASSERT_TRUE(child.Interrupt(error)) << error;
    ASSERT_TRUE(child.WaitForExit(30s));
    EXPECT_EQ(child.GetExit()->Code, std::optional<int64>(0));
    EXPECT_NE(ReadWhole(directory.Path() / "out.log").find("stopped by signal"), std::string::npos);
    EXPECT_FALSE(child.Interrupt(error));
}

TEST(ChildProcessHandleTest, EndTreeEndsTheChildAndItsGrandchild)
{
    LogTestDirectory directory;
    std::string error;
    ChildProcessHandle child = ChildProcessHandle::Launch(LaunchOptions(directory, { "spawn-sleeper", "sleep", "60000" }), error);
    ASSERT_TRUE(child) << error;
    ASSERT_TRUE(WaitForText(directory.Path() / "out.log", "\n"));
    ASSERT_TRUE(child.EndTree(error)) << error;
    ASSERT_TRUE(child.WaitForExit(30s));
    std::vector<std::string> const lines = Lines(directory.Path() / "out.log");
    ExpectGrandchildEnded(lines);
}

TEST(ChildProcessHandleTest, AdoptTakesARunningChildBackOnlyWhileItsIdentityMatches)
{
    LogTestDirectory directory;
    std::string error;
    ChildProcessIdentity identity;
    {
        ChildProcessHandle child = ChildProcessHandle::Launch(LaunchOptions(directory, { "spawn-sleeper", "sleep", "60000" }), error);
        ASSERT_TRUE(child) << error;
        identity = child.GetIdentity();
        ASSERT_TRUE(WaitForText(directory.Path() / "out.log", "\n"));
    }
    std::optional<ChildProcessIdentity> const described = ChildProcessHandle::Describe(identity.Id);
    ASSERT_TRUE(described.has_value());
    EXPECT_TRUE(described->Matches(identity));

    ChildProcessIdentity laterStart = identity;
    laterStart.StartTime += 1;
    EXPECT_FALSE(ChildProcessHandle::Adopt(laterStart, {}, error));
    EXPECT_NE(error.find(std::to_string(identity.Id)), std::string::npos) << error;
    ChildProcessIdentity otherProgram = identity;
    otherProgram.Executable = otherProgram.Executable.parent_path() / "another-program";
    EXPECT_FALSE(ChildProcessHandle::Adopt(otherProgram, {}, error));

    ChildProcessHandle adopted = ChildProcessHandle::Adopt(identity, {}, error);
    ASSERT_TRUE(adopted) << error;
    ScopeExit const cleanup([&adopted] { std::string ignored; adopted.EndTree(ignored); });
    EXPECT_TRUE(adopted.IsAdopted());
    EXPECT_FALSE(adopted.HasInput());
    EXPECT_FALSE(adopted.WaitForExit(0ms));
    ASSERT_TRUE(adopted.EndTree(error)) << error;
    ASSERT_TRUE(adopted.WaitForExit(30s));
#ifdef _WIN32
    EXPECT_EQ(adopted.GetExit()->Code, std::optional<int64>(1));
#endif
    std::vector<std::string> const lines = Lines(directory.Path() / "out.log");
    ExpectGrandchildEnded(lines);
}

TEST(ChildProcessHandleTest, LaunchReportsAProgramThatCannotStart)
{
    LogTestDirectory directory;
    ChildLaunchOptions options = LaunchOptions(directory, {});
    options.Program = directory.Path() / "missing-program";
    std::string error;
    EXPECT_FALSE(ChildProcessHandle::Launch(options, error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(Utf::IsValidUtf8(error));
    options.Program.clear();
    EXPECT_FALSE(ChildProcessHandle::Launch(options, error));
    EXPECT_FALSE(ChildProcessHandle::Describe(0).has_value());
}
