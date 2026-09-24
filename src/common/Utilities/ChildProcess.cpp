/*
 * Project Ambrose by Imjustchico
 * Implements running a child process: on Windows CreateProcessW with a command line quoted by CommandLineToArgvW rules, CREATE_NO_WINDOW unless the program draws its own window, only the input, which is the NUL device or the read end of an anonymous pipe whose write end stays uninheritable here, and two overlapped named pipes inherited, and a kill-on-close job object, running <path>.exe for a program path without an extension when only that file exists, refusing batch files because cmd.exe reparses their arguments, and wording system errors in UTF-8; on POSIX posix_spawn into a new process group with /dev/null or the read end of a close-on-exec pipe as input and output pipes read with poll, ending the group with SIGTERM then SIGKILL, after clearing a SIGCHLD disposition that would reap the child before its exit code is read; a program named without a folder is searched for on the PATH, and output is split into UTF-8 lines, with invalid bytes replaced, on the calling thread. StartDetached shares that command building and starts a program nobody waits for: on Windows a detached process of its own group, breaking away from a job when the job allows it, and on POSIX posix_spawn into a session of its own with every standard handle on the null device. ExitWhenInputEnds reads standard input on a detached thread and ends the process with no cleanup once it reaches its end or fails. A ChildProcessHandle launch opens the output and error files for appending and hands them over as the standard handles, keeps the write end of the input pipe non-blocking, and on Windows creates the process suspended with no window in a process group of its own, breaking away from an outer job when that job allows it, then puts it in a job named after its process id and start time that ends nothing when this process closes it, and leaves a handle to that job inside the child, because a name lives only while a handle does, so a later process can open that job by name to end the tree for as long as the child runs, and reads its identity from the process times and image name; on POSIX it spawns into a session of its own, reads the identity from /proc with the boot id beside it and a deleted executable's suffix dropped, watches the exit with a pidfd where the kernel has one, and blocks SIGPIPE on the writing thread so a closed input never ends this process. SendConsoleBreak, for a helper process only, leaves its own console, attaches to the target's and sends Ctrl+Break to its group.
 */

#include "ChildProcess.h"
#include "StringUtil.h"
#include "Types.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

extern char** environ;

#ifdef __GLIBC__
#if __GLIBC_PREREQ(2, 29)
#define AMBROSE_SPAWN_CHDIR 1
#endif
#if __GLIBC_PREREQ(2, 34)
#define AMBROSE_SPAWN_CLOSEFROM 1
#endif
#elif defined(__APPLE__)
#define AMBROSE_SPAWN_CHDIR 1
#endif
#endif

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t ReadBufferBytes = 64 * 1024;
    constexpr std::size_t InputWatchBytes = 512;

    std::string PathText(std::filesystem::path const& path)
    {
#ifdef _WIN32
        std::wstring const& native = path.native();
        return Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(native.data()), native.size()), Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
#else
        return path.native();
#endif
    }

    std::chrono::milliseconds WaitUntil(Clock::time_point now, Clock::time_point wake)
    {
        if (wake <= now)
            return std::chrono::milliseconds(0);
        return std::chrono::ceil<std::chrono::milliseconds>(wake - now);
    }

    class LineSplitter
    {
    public:
        LineSplitter(ChildProcessOptions const& options, bool error) : _options(options), _error(error)
        {
        }

        void Feed(std::string_view bytes)
        {
            _pending.append(bytes.data(), bytes.size());
            std::size_t begin = 0;
            std::size_t search = _scanned;
            while (true)
            {
                std::size_t const newline = _pending.find('\n', search);
                if (newline == std::string::npos)
                    break;
                EmitLine(std::string_view(_pending).substr(begin, newline - begin));
                begin = newline + 1;
                search = begin;
            }
            while (_pending.size() - begin > ChildProcess::MaxLineBytes + 1)
            {
                std::string_view const rest = std::string_view(_pending).substr(begin);
                std::string_view piece = Ambrose::TruncateUtf8(rest, ChildProcess::MaxLineBytes);
                if (piece.empty())
                    piece = rest.substr(0, ChildProcess::MaxLineBytes);
                EmitText(piece);
                begin += piece.size();
            }
            _pending.erase(0, begin);
            _scanned = _pending.size();
        }

        void Finish()
        {
            if (_pending.empty())
                return;
            std::string const rest = std::move(_pending);
            _pending.clear();
            _scanned = 0;
            EmitLine(rest);
        }

    private:
        void EmitLine(std::string_view line)
        {
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            EmitText(line);
        }

        void EmitText(std::string_view text)
        {
            if (!_options.OnLine)
                return;
            std::string sanitized;
            if (!Utf::IsValidUtf8(text))
            {
                std::u16string const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::u16string());
                sanitized = Utf::Utf16ToUtf8(wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
                text = sanitized;
            }
            do
            {
                std::string_view piece = Ambrose::TruncateUtf8(text, ChildProcess::MaxLineBytes);
                if (piece.empty() && !text.empty())
                    piece = text.substr(0, ChildProcess::MaxLineBytes);
                _options.OnLine(piece, _error);
                text.remove_prefix(piece.size());
            } while (!text.empty());
        }

        ChildProcessOptions const& _options;
        bool _error;
        std::string _pending;
        std::size_t _scanned = 0;
    };

    enum class Action
    {
        None,
        Terminate,
        Kill,
        GiveUp,
        EndLeftovers,
        AbandonOutput
    };

    class Supervisor
    {
    public:
        Supervisor(ChildProcessOptions const& options, ChildProcessResult& result)
            : _options(options), _result(result), _started(Clock::now()), _nextStopCheck(_started), _phaseStart(_started)
        {
        }

        std::chrono::milliseconds NextWait() const
        {
            Clock::time_point const now = Clock::now();
            Clock::time_point wake = now + ChildProcess::PollInterval;
            if (!_exited && !_terminated && _options.Timeout.count() > 0)
                wake = std::min(wake, _started + _options.Timeout);
            else if (_exited || _terminated)
                wake = std::min(wake, _phaseStart + (_exited ? ChildProcess::OutputDrainGrace : ChildProcess::TerminateGrace));
            return WaitUntil(now, wake);
        }

        void MarkExited()
        {
            if (_exited)
                return;
            _exited = true;
            _phaseStart = Clock::now();
        }

        bool Exited() const { return _exited; }
        bool Terminated() const { return _terminated; }

        Action Next(bool outputOpen)
        {
            Clock::time_point const now = Clock::now();
            if (_exited)
            {
                if (!outputOpen || now - _phaseStart < ChildProcess::OutputDrainGrace)
                    return Action::None;
                _phaseStart = now;
                if (!_endedLeftovers)
                {
                    _endedLeftovers = true;
                    return Action::EndLeftovers;
                }
                return Action::AbandonOutput;
            }
            if (!_terminated)
            {
                if (_options.Timeout.count() > 0 && now - _started >= _options.Timeout)
                {
                    _result.TimedOut = true;
                }
                else if (_options.ShouldStop && now >= _nextStopCheck)
                {
                    _nextStopCheck = now + ChildProcess::PollInterval;
                    _result.Stopped = _options.ShouldStop();
                }
                if (!_result.TimedOut && !_result.Stopped)
                    return Action::None;
                _terminated = true;
                _phaseStart = now;
                return Action::Terminate;
            }
            if (now - _phaseStart < ChildProcess::TerminateGrace)
                return Action::None;
            _phaseStart = now;
            if (!_killed)
            {
                _killed = true;
                return Action::Kill;
            }
            return Action::GiveUp;
        }

    private:
        ChildProcessOptions const& _options;
        ChildProcessResult& _result;
        Clock::time_point _started;
        Clock::time_point _nextStopCheck;
        Clock::time_point _phaseStart;
        bool _exited = false;
        bool _terminated = false;
        bool _killed = false;
        bool _endedLeftovers = false;
    };

    std::string CheckOptions(ChildProcessOptions const& options, std::string const& programText)
    {
        if (options.Program.empty())
            return "no program was named to run";
        for (std::size_t index = 0; index < options.Arguments.size(); ++index)
        {
            if (options.Arguments[index].find('\0') != std::string::npos)
                return fmt::format("argument {} for {} holds a NUL character, which no program can receive", index + 1, programText);
        }
        return std::string();
    }

    bool MakeAbsolute(std::filesystem::path& path, std::string_view what, ChildProcessResult& result)
    {
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::absolute(path, error);
        if (error)
        {
            result.Error = fmt::format("the {} {} could not be made absolute: {}", what, PathText(path), error.message());
            return false;
        }
        path = std::move(absolute);
        return true;
    }

#ifdef _WIN32
    constexpr DWORD PipeBufferBytes = 64 * 1024;
    constexpr std::size_t MaxCommandLineCharacters = 32767;

    std::string SystemMessage(DWORD code)
    {
        LPWSTR buffer = nullptr;
        DWORD const length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
        std::string text;
        if (length != 0 && buffer != nullptr)
            text = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(buffer), length), Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
        if (buffer != nullptr)
            LocalFree(buffer);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\r' || text.back() == '\n' || text.back() == '\t'))
            text.pop_back();
        if (text.empty())
            return fmt::format("Windows error {}", code);
        return text;
    }

    void PreferExecutableExtension(std::filesystem::path& program)
    {
        if (program.has_extension())
            return;
        std::error_code error;
        if (std::filesystem::is_regular_file(program, error))
            return;
        std::filesystem::path withExtension = program;
        withExtension += L".exe";
        if (std::filesystem::is_regular_file(withExtension, error))
            program = std::move(withExtension);
    }

    class Handle
    {
    public:
        Handle() noexcept = default;

        explicit Handle(HANDLE handle) noexcept : _handle(handle == INVALID_HANDLE_VALUE ? nullptr : handle)
        {
        }

        ~Handle()
        {
            Reset();
        }

        Handle(Handle const&) = delete;
        Handle& operator=(Handle const&) = delete;

        Handle(Handle&& other) noexcept : _handle(std::exchange(other._handle, nullptr))
        {
        }

        Handle& operator=(Handle&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _handle = std::exchange(other._handle, nullptr);
            }
            return *this;
        }

        HANDLE Get() const noexcept { return _handle; }
        explicit operator bool() const noexcept { return _handle != nullptr; }

        void Reset() noexcept
        {
            if (_handle != nullptr)
            {
                CloseHandle(_handle);
                _handle = nullptr;
            }
        }

    private:
        HANDLE _handle = nullptr;
    };

    class PipeReader
    {
    public:
        PipeReader(ChildProcessOptions const& options, bool error) : _lines(options, error), _buffer(ReadBufferBytes)
        {
        }

        ~PipeReader()
        {
            CancelPending(false);
        }

        PipeReader(PipeReader const&) = delete;
        PipeReader& operator=(PipeReader const&) = delete;

        std::string Create(Handle& childEnd)
        {
            static std::atomic<uint64> counter{ 0 };
            std::wstring const name = L"\\\\.\\pipe\\ProjectAmbrose.ChildProcess." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(counter.fetch_add(1));
            _event = Handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!_event)
                return fmt::format("an event could not be created: {}", SystemMessage(GetLastError()));
            _pipe = Handle(CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, PipeBufferBytes, PipeBufferBytes, 0, nullptr));
            if (!_pipe)
                return fmt::format("a pipe could not be created: {}", SystemMessage(GetLastError()));
            childEnd = Handle(CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr));
            if (!childEnd)
                return fmt::format("a pipe could not be opened: {}", SystemMessage(GetLastError()));
            return std::string();
        }

        bool IsOpen() const noexcept { return static_cast<bool>(_pipe); }
        HANDLE Event() const noexcept { return _event.Get(); }

        void Start()
        {
            if (!_pipe || _pending)
                return;
            _overlapped = OVERLAPPED{};
            _overlapped.hEvent = _event.Get();
            if (ReadFile(_pipe.Get(), _buffer.data(), static_cast<DWORD>(_buffer.size()), nullptr, &_overlapped) || GetLastError() == ERROR_IO_PENDING)
            {
                _pending = true;
                return;
            }
            End();
        }

        void Collect()
        {
            if (!_pending || WaitForSingleObject(_event.Get(), 0) != WAIT_OBJECT_0)
                return;
            DWORD bytes = 0;
            if (!GetOverlappedResult(_pipe.Get(), &_overlapped, &bytes, FALSE))
            {
                if (GetLastError() == ERROR_IO_INCOMPLETE)
                    return;
                _pending = false;
                End();
                return;
            }
            _pending = false;
            _lines.Feed(std::string_view(_buffer.data(), bytes));
            Start();
        }

        void Abandon()
        {
            if (!_pipe)
                return;
            CancelPending(true);
            End();
        }

    private:
        void CancelPending(bool deliver)
        {
            if (!_pending)
                return;
            _pending = false;
            CancelIoEx(_pipe.Get(), &_overlapped);
            DWORD bytes = 0;
            if (GetOverlappedResult(_pipe.Get(), &_overlapped, &bytes, TRUE) && deliver && bytes > 0)
                _lines.Feed(std::string_view(_buffer.data(), bytes));
        }

        void End()
        {
            _pipe.Reset();
            _lines.Finish();
        }

        LineSplitter _lines;
        std::vector<char> _buffer;
        Handle _pipe;
        Handle _event;
        OVERLAPPED _overlapped{};
        bool _pending = false;
    };

    class AttributeList
    {
    public:
        explicit AttributeList(std::array<HANDLE, 3>& inherited)
        {
            SIZE_T bytes = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
            _storage.resize(bytes);
            LPPROC_THREAD_ATTRIBUTE_LIST const list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(_storage.data());
            if (bytes == 0 || !InitializeProcThreadAttributeList(list, 1, 0, &bytes))
            {
                _error = GetLastError();
                return;
            }
            _initialized = true;
            if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(), sizeof(HANDLE) * inherited.size(), nullptr, nullptr))
            {
                _error = GetLastError();
                return;
            }
            _list = list;
        }

        ~AttributeList()
        {
            if (_initialized)
                DeleteProcThreadAttributeList(reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(_storage.data()));
        }

        AttributeList(AttributeList const&) = delete;
        AttributeList& operator=(AttributeList const&) = delete;

        LPPROC_THREAD_ATTRIBUTE_LIST Get() const noexcept { return _list; }
        DWORD Error() const noexcept { return _error; }

    private:
        std::vector<unsigned char> _storage;
        LPPROC_THREAD_ATTRIBUTE_LIST _list = nullptr;
        DWORD _error = 0;
        bool _initialized = false;
    };

    bool IsBatchFile(std::filesystem::path const& program)
    {
        std::wstring const extension = program.extension().native();
        return CompareStringOrdinal(extension.c_str(), -1, L".bat", -1, TRUE) == CSTR_EQUAL
            || CompareStringOrdinal(extension.c_str(), -1, L".cmd", -1, TRUE) == CSTR_EQUAL;
    }

    void EndProcesses(Handle const& job, Handle const& process)
    {
        if (!job || !TerminateJobObject(job.Get(), 1))
            TerminateProcess(process.Get(), 1);
    }

    struct WindowsCommand
    {
        std::filesystem::path Program;
        std::wstring CommandBuffer;
        std::filesystem::path WorkingDirectory;
        bool Search = false;
    };

    bool PrepareCommand(ChildProcessOptions const& options, std::string const& programText, WindowsCommand& command, ChildProcessResult& result)
    {
        std::filesystem::path program = options.Program;
        bool const search = !program.has_parent_path();
        if (!search && !MakeAbsolute(program, "program", result))
            return false;
        if (!search)
            PreferExecutableExtension(program);
        if (IsBatchFile(program))
        {
            result.Error = fmt::format("{} is a batch file, and batch files are not run because cmd.exe does not keep arguments exactly as given", programText);
            return false;
        }
        std::optional<std::string> const programUtf8 = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(program.native().data()), program.native().size()), Utf::InvalidPolicy::Reject);
        if (!programUtf8)
        {
            result.Error = fmt::format("the program path {} is not valid Unicode", programText);
            return false;
        }
        std::string commandLine = ChildProcess::QuoteWindowsArgument(*programUtf8);
        for (std::size_t index = 0; index < options.Arguments.size(); ++index)
        {
            if (!Utf::IsValidUtf8(options.Arguments[index]))
            {
                result.Error = fmt::format("argument {} for {} is not valid UTF-8", index + 1, programText);
                return false;
            }
            commandLine.push_back(' ');
            commandLine.append(ChildProcess::QuoteWindowsArgument(options.Arguments[index]));
        }
        std::optional<std::u16string> const wideCommandLine = Utf::Utf8ToUtf16(commandLine, Utf::InvalidPolicy::Reject);
        if (!wideCommandLine)
        {
            result.Error = fmt::format("the command line for {} is not valid UTF-8", programText);
            return false;
        }
        if (wideCommandLine->size() >= MaxCommandLineCharacters)
        {
            result.Error = fmt::format("the command line for {} is {} characters long, and Windows allows at most {}", programText, wideCommandLine->size(), MaxCommandLineCharacters - 1);
            return false;
        }
        std::filesystem::path workingDirectory = options.WorkingDirectory;
        if (!workingDirectory.empty() && !MakeAbsolute(workingDirectory, "working directory", result))
            return false;
        command.Program = std::move(program);
        command.CommandBuffer.assign(reinterpret_cast<wchar_t const*>(wideCommandLine->data()), wideCommandLine->size());
        command.WorkingDirectory = std::move(workingDirectory);
        command.Search = search;
        return true;
    }

    void StartDetachedPlatform(ChildProcessOptions const& options, std::string const& programText, ChildProcessResult& result)
    {
        WindowsCommand command;
        if (!PrepareCommand(options, programText, command, result))
            return;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION information{};
        std::wstring commandBuffer = command.CommandBuffer;
        BOOL const created = CreateProcessW(command.Search ? nullptr : command.Program.c_str(), commandBuffer.data(), nullptr, nullptr, FALSE,
            CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS, nullptr,
            command.WorkingDirectory.empty() ? nullptr : command.WorkingDirectory.c_str(), &startup, &information);
        DWORD const createError = GetLastError();
        if (!created && createError == ERROR_ACCESS_DENIED)
        {
            commandBuffer = command.CommandBuffer;
            if (CreateProcessW(command.Search ? nullptr : command.Program.c_str(), commandBuffer.data(), nullptr, nullptr, FALSE,
                CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, nullptr,
                command.WorkingDirectory.empty() ? nullptr : command.WorkingDirectory.c_str(), &startup, &information))
            {
                CloseHandle(information.hThread);
                CloseHandle(information.hProcess);
                result.Started = true;
                return;
            }
            result.Error = fmt::format("{} could not be started: {}", programText, SystemMessage(GetLastError()));
            return;
        }
        if (!created)
        {
            result.Error = fmt::format("{} could not be started: {}", programText, SystemMessage(createError));
            return;
        }
        CloseHandle(information.hThread);
        CloseHandle(information.hProcess);
        result.Started = true;
    }

    void RunPlatform(ChildProcessOptions const& options, std::string const& programText, ChildProcessResult& result)
    {
        WindowsCommand command;
        if (!PrepareCommand(options, programText, command, result))
            return;
        std::filesystem::path const& program = command.Program;
        bool const search = command.Search;
        std::wstring commandBuffer = command.CommandBuffer;
        std::filesystem::path const& workingDirectory = command.WorkingDirectory;

        PipeReader output(options, false);
        PipeReader errors(options, true);
        Handle outputChild;
        Handle errorChild;
        if (std::string const error = output.Create(outputChild); !error.empty())
        {
            result.Error = fmt::format("{} could not be started because its standard output {}", programText, error);
            return;
        }
        if (std::string const error = errors.Create(errorChild); !error.empty())
        {
            result.Error = fmt::format("{} could not be started because its standard error {}", programText, error);
            return;
        }
        Handle input;
        Handle inputHeld;
        if (options.InputEndsWithParent)
        {
            HANDLE readEnd = nullptr;
            HANDLE writeEnd = nullptr;
            if (!CreatePipe(&readEnd, &writeEnd, nullptr, 0))
            {
                result.Error = fmt::format("{} could not be started because a pipe for its input could not be created: {}", programText, SystemMessage(GetLastError()));
                return;
            }
            input = Handle(readEnd);
            inputHeld = Handle(writeEnd);
        }
        else
        {
            input = Handle(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!input)
            {
                result.Error = fmt::format("{} could not be started because the NUL device could not be opened for its input: {}", programText, SystemMessage(GetLastError()));
                return;
            }
        }
        std::array<HANDLE, 3> inherited{ input.Get(), outputChild.Get(), errorChild.Get() };
        for (HANDLE const handle : inherited)
        {
            if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
            {
                result.Error = fmt::format("{} could not be started because a handle could not be made inheritable: {}", programText, SystemMessage(GetLastError()));
                return;
            }
        }
        AttributeList attributes(inherited);
        if (attributes.Get() == nullptr)
        {
            result.Error = fmt::format("{} could not be started because its handle list could not be built: {}", programText, SystemMessage(attributes.Error()));
            return;
        }

        Handle job(CreateJobObjectW(nullptr, nullptr));
        if (job)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
            if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                job.Reset();
        }

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = options.ShowsWindow ? STARTF_USESTDHANDLES : (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
        startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.StartupInfo.hStdInput = input.Get();
        startup.StartupInfo.hStdOutput = outputChild.Get();
        startup.StartupInfo.hStdError = errorChild.Get();
        startup.lpAttributeList = attributes.Get();
        DWORD const creation = (options.ShowsWindow ? 0u : DWORD{ CREATE_NO_WINDOW }) | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT;
        PROCESS_INFORMATION information{};
        BOOL const created = CreateProcessW(search ? nullptr : program.c_str(), commandBuffer.data(), nullptr, nullptr, TRUE,
            creation, nullptr, workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            &startup.StartupInfo, &information);
        DWORD const createError = GetLastError();
        input.Reset();
        outputChild.Reset();
        errorChild.Reset();
        if (!created)
        {
            result.Error = fmt::format("{} could not be started: {}", programText, SystemMessage(createError));
            return;
        }
        Handle process(information.hProcess);
        Handle thread(information.hThread);
        if (job && !AssignProcessToJobObject(job.Get(), process.Get()))
            job.Reset();
        if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1))
        {
            DWORD const resumeError = GetLastError();
            TerminateProcess(process.Get(), 1);
            result.Error = fmt::format("{} could not be started because its first thread could not be resumed: {}", programText, SystemMessage(resumeError));
            return;
        }
        thread.Reset();
        result.Started = true;

        std::array<PipeReader*, 2> const readers{ &output, &errors };
        Supervisor supervisor(options, result);
        output.Start();
        errors.Start();
        while (true)
        {
            if (supervisor.Exited() && !output.IsOpen() && !errors.IsOpen())
                break;
            std::array<HANDLE, 3> waits{};
            DWORD count = 0;
            if (!supervisor.Exited())
                waits[count++] = process.Get();
            for (PipeReader* const reader : readers)
            {
                if (reader->IsOpen())
                    waits[count++] = reader->Event();
            }
            if (WaitForMultipleObjects(count, waits.data(), FALSE, static_cast<DWORD>(supervisor.NextWait().count())) == WAIT_FAILED)
                Sleep(static_cast<DWORD>(ChildProcess::PollInterval.count()));
            for (PipeReader* const reader : readers)
                reader->Collect();
            if (!supervisor.Exited() && WaitForSingleObject(process.Get(), 0) == WAIT_OBJECT_0)
                supervisor.MarkExited();
            Action const action = supervisor.Next(output.IsOpen() || errors.IsOpen());
            if (action == Action::Terminate || action == Action::Kill || action == Action::EndLeftovers)
            {
                EndProcesses(job, process);
            }
            else if (action == Action::AbandonOutput)
            {
                output.Abandon();
                errors.Abandon();
            }
            else if (action == Action::GiveUp)
            {
                result.Error = fmt::format("{} did not end after it was told to", programText);
                break;
            }
        }
        if (supervisor.Exited() && !supervisor.Terminated())
        {
            DWORD code = 0;
            if (GetExitCodeProcess(process.Get(), &code))
                result.ExitCode = static_cast<int>(code);
            else
                result.Error = fmt::format("the exit code of {} could not be read: {}", programText, SystemMessage(GetLastError()));
        }
        if (job)
            TerminateJobObject(job.Get(), 1);
    }
#else
    constexpr std::array<int, 9> DefaultSignals{ SIGPIPE, SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGCHLD, SIGUSR1, SIGUSR2, SIGALRM };
    constexpr std::chrono::milliseconds FirstExitWait{ 1 };

    std::string ErrnoMessage(int code)
    {
        return std::generic_category().message(code);
    }

    class Descriptor
    {
    public:
        Descriptor() noexcept = default;

        explicit Descriptor(int descriptor) noexcept : _descriptor(descriptor)
        {
        }

        ~Descriptor()
        {
            Reset();
        }

        Descriptor(Descriptor const&) = delete;
        Descriptor& operator=(Descriptor const&) = delete;

        Descriptor(Descriptor&& other) noexcept : _descriptor(std::exchange(other._descriptor, -1))
        {
        }

        Descriptor& operator=(Descriptor&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _descriptor = std::exchange(other._descriptor, -1);
            }
            return *this;
        }

        int Get() const noexcept { return _descriptor; }
        explicit operator bool() const noexcept { return _descriptor >= 0; }

        void Reset() noexcept
        {
            if (_descriptor >= 0)
            {
                close(_descriptor);
                _descriptor = -1;
            }
        }

    private:
        int _descriptor = -1;
    };

    int MakePipe(Descriptor& readEnd, Descriptor& writeEnd, bool nonBlockingRead)
    {
        std::array<int, 2> descriptors{ -1, -1 };
#ifdef __linux__
        if (pipe2(descriptors.data(), O_CLOEXEC) != 0)
            return errno;
        readEnd = Descriptor(descriptors[0]);
        writeEnd = Descriptor(descriptors[1]);
#else
        if (pipe(descriptors.data()) != 0)
            return errno;
        readEnd = Descriptor(descriptors[0]);
        writeEnd = Descriptor(descriptors[1]);
        if (fcntl(readEnd.Get(), F_SETFD, FD_CLOEXEC) != 0 || fcntl(writeEnd.Get(), F_SETFD, FD_CLOEXEC) != 0)
            return errno;
#endif
        std::array<Descriptor*, 2> const ends{ &readEnd, &writeEnd };
        for (Descriptor* const end : ends)
        {
            if (end->Get() > STDERR_FILENO)
                continue;
            int const moved = fcntl(end->Get(), F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
            if (moved < 0)
                return errno;
            *end = Descriptor(moved);
        }
        if (!nonBlockingRead)
            return 0;
        int const flags = fcntl(readEnd.Get(), F_GETFL);
        if (flags < 0 || fcntl(readEnd.Get(), F_SETFL, flags | O_NONBLOCK) != 0)
            return errno;
        return 0;
    }

    int KeepChildrenWaitable()
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> const lock(mutex);
        struct sigaction current{};
        if (sigaction(SIGCHLD, nullptr, &current) != 0)
            return errno;
        bool const ignored = (current.sa_flags & SA_SIGINFO) == 0 && current.sa_handler == SIG_IGN;
        if (!ignored && (current.sa_flags & SA_NOCLDWAIT) == 0)
            return 0;
        if (ignored)
            current.sa_handler = SIG_DFL;
        current.sa_flags &= ~SA_NOCLDWAIT;
        return sigaction(SIGCHLD, &current, nullptr) == 0 ? 0 : errno;
    }

    class PipeReader
    {
    public:
        PipeReader(ChildProcessOptions const& options, bool error) : _lines(options, error), _buffer(ReadBufferBytes)
        {
        }

        Descriptor& Pipe() noexcept { return _pipe; }
        bool IsOpen() const noexcept { return static_cast<bool>(_pipe); }
        int Get() const noexcept { return _pipe.Get(); }

        void Read()
        {
            while (_pipe)
            {
                ssize_t const bytes = read(_pipe.Get(), _buffer.data(), _buffer.size());
                if (bytes > 0)
                {
                    _lines.Feed(std::string_view(_buffer.data(), static_cast<std::size_t>(bytes)));
                    return;
                }
                if (bytes < 0 && errno == EINTR)
                    continue;
                if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    return;
                Abandon();
            }
        }

        void Abandon()
        {
            if (!_pipe)
                return;
            _pipe.Reset();
            _lines.Finish();
        }

    private:
        LineSplitter _lines;
        std::vector<char> _buffer;
        Descriptor _pipe;
    };

    class SpawnSetup
    {
    public:
        SpawnSetup()
        {
            _actionsReady = posix_spawn_file_actions_init(&_actions) == 0;
            _attributesReady = posix_spawnattr_init(&_attributes) == 0;
        }

        ~SpawnSetup()
        {
            if (_actionsReady)
                posix_spawn_file_actions_destroy(&_actions);
            if (_attributesReady)
                posix_spawnattr_destroy(&_attributes);
        }

        SpawnSetup(SpawnSetup const&) = delete;
        SpawnSetup& operator=(SpawnSetup const&) = delete;

        bool Ready() const noexcept { return _actionsReady && _attributesReady; }
        posix_spawn_file_actions_t* Actions() noexcept { return &_actions; }
        posix_spawnattr_t* Attributes() noexcept { return &_attributes; }

    private:
        posix_spawn_file_actions_t _actions{};
        posix_spawnattr_t _attributes{};
        bool _actionsReady = false;
        bool _attributesReady = false;
    };

    class ChildGroup
    {
    public:
        explicit ChildGroup(pid_t id) noexcept : _id(id)
        {
        }

        ~ChildGroup()
        {
            if (_reaped || _abandoned)
                return;
            Signal(SIGKILL);
            int error = 0;
            Reap(error);
        }

        ChildGroup(ChildGroup const&) = delete;
        ChildGroup& operator=(ChildGroup const&) = delete;

        void Signal(int number) noexcept
        {
            if (_reaped)
                return;
            kill(-_id, number);
            kill(_id, number);
        }

        bool HasExited(std::string& error)
        {
            while (true)
            {
                siginfo_t information{};
                information.si_pid = 0;
                if (waitid(P_PID, static_cast<id_t>(_id), &information, WEXITED | WNOHANG | WNOWAIT) == 0)
                    return information.si_pid == _id;
                if (errno == EINTR)
                    continue;
                _lostError = errno;
                _reaped = true;
                error = ErrnoMessage(_lostError);
                return true;
            }
        }

        std::optional<int> Reap(int& error)
        {
            int status = 0;
            error = _lostError;
            if (_reaped)
                return std::nullopt;
            while (waitpid(_id, &status, 0) < 0)
            {
                if (errno != EINTR)
                {
                    error = errno;
                    _reaped = true;
                    return std::nullopt;
                }
            }
            _reaped = true;
            return status;
        }

        void Abandon() noexcept
        {
            _abandoned = true;
        }

    private:
        pid_t _id;
        int _lostError = 0;
        bool _reaped = false;
        bool _abandoned = false;
    };

    std::vector<char*> BuildArgv(std::filesystem::path const& program, ChildProcessOptions const& options, std::vector<std::string>& strings)
    {
        strings.reserve(options.Arguments.size() + 1);
        strings.push_back(program.native());
        strings.insert(strings.end(), options.Arguments.begin(), options.Arguments.end());
        std::vector<char*> argv;
        argv.reserve(strings.size() + 1);
        for (std::string& text : strings)
            argv.push_back(text.data());
        argv.push_back(nullptr);
        return argv;
    }

    void StartDetachedPlatform(ChildProcessOptions const& options, std::string const& programText, ChildProcessResult& result)
    {
        std::filesystem::path program = options.Program;
        bool const search = program.native().find('/') == std::string::npos;
        if (!search && !MakeAbsolute(program, "program", result))
            return;
        std::filesystem::path workingDirectory = options.WorkingDirectory;
        if (!workingDirectory.empty() && !MakeAbsolute(workingDirectory, "working directory", result))
            return;
#ifndef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
        {
            result.Error = fmt::format("{} could not be started because this system cannot start a program in another working directory", programText);
            return;
        }
#endif
        SpawnSetup setup;
        if (!setup.Ready())
        {
            result.Error = fmt::format("{} could not be started because its spawn settings could not be prepared", programText);
            return;
        }
        int setupCode = 0;
        auto const step = [&setupCode](int code)
        {
            if (setupCode == 0)
                setupCode = code;
        };
        step(posix_spawn_file_actions_addopen(setup.Actions(), STDIN_FILENO, "/dev/null", O_RDONLY, 0));
        step(posix_spawn_file_actions_addopen(setup.Actions(), STDOUT_FILENO, "/dev/null", O_WRONLY, 0));
        step(posix_spawn_file_actions_addopen(setup.Actions(), STDERR_FILENO, "/dev/null", O_WRONLY, 0));
#ifdef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
            step(posix_spawn_file_actions_addchdir_np(setup.Actions(), workingDirectory.c_str()));
#endif
#ifdef AMBROSE_SPAWN_CLOSEFROM
        step(posix_spawn_file_actions_addclosefrom_np(setup.Actions(), STDERR_FILENO + 1));
#endif
        int flags = POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
#ifdef POSIX_SPAWN_SETSID
        flags |= POSIX_SPAWN_SETSID;
#else
        flags |= POSIX_SPAWN_SETPGROUP;
        step(posix_spawnattr_setpgroup(setup.Attributes(), 0));
#endif
#ifdef __APPLE__
        flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
        step(posix_spawnattr_setflags(setup.Attributes(), static_cast<short>(flags)));
        sigset_t mask{};
        sigemptyset(&mask);
        step(posix_spawnattr_setsigmask(setup.Attributes(), &mask));
        sigset_t defaults{};
        sigemptyset(&defaults);
        for (int const number : DefaultSignals)
            sigaddset(&defaults, number);
        step(posix_spawnattr_setsigdefault(setup.Attributes(), &defaults));
        if (setupCode != 0)
        {
            result.Error = fmt::format("{} could not be started because its spawn settings could not be prepared: {}", programText, ErrnoMessage(setupCode));
            return;
        }

        std::vector<std::string> strings;
        std::vector<char*> argv = BuildArgv(program, options, strings);
        pid_t id = 0;
        int const spawned = search
            ? posix_spawnp(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ)
            : posix_spawn(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ);
        if (spawned != 0)
        {
            result.Error = fmt::format("{} could not be started: {}", programText, ErrnoMessage(spawned));
            return;
        }
        result.Started = true;
    }

    void RunPlatform(ChildProcessOptions const& options, std::string const& programText, ChildProcessResult& result)
    {
        std::filesystem::path program = options.Program;
        bool const search = program.native().find('/') == std::string::npos;
        if (!search && !MakeAbsolute(program, "program", result))
            return;
        std::filesystem::path workingDirectory = options.WorkingDirectory;
        if (!workingDirectory.empty() && !MakeAbsolute(workingDirectory, "working directory", result))
            return;
#ifndef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
        {
            result.Error = fmt::format("{} could not be started because this system cannot start a program in another working directory", programText);
            return;
        }
#endif
        if (int const code = KeepChildrenWaitable(); code != 0)
        {
            result.Error = fmt::format("{} could not be started because SIGCHLD would reap it before its exit code is read, and that could not be changed: {}", programText, ErrnoMessage(code));
            return;
        }

        PipeReader output(options, false);
        PipeReader errors(options, true);
        Descriptor outputWrite;
        Descriptor errorWrite;
        Descriptor inputRead;
        Descriptor inputHeld;
        if (int const code = MakePipe(output.Pipe(), outputWrite, true); code != 0)
        {
            result.Error = fmt::format("{} could not be started because a pipe for its standard output could not be created: {}", programText, ErrnoMessage(code));
            return;
        }
        if (int const code = MakePipe(errors.Pipe(), errorWrite, true); code != 0)
        {
            result.Error = fmt::format("{} could not be started because a pipe for its standard error could not be created: {}", programText, ErrnoMessage(code));
            return;
        }
        if (options.InputEndsWithParent)
        {
            if (int const code = MakePipe(inputRead, inputHeld, false); code != 0)
            {
                result.Error = fmt::format("{} could not be started because a pipe for its input could not be created: {}", programText, ErrnoMessage(code));
                return;
            }
        }

        SpawnSetup setup;
        if (!setup.Ready())
        {
            result.Error = fmt::format("{} could not be started because its spawn settings could not be prepared", programText);
            return;
        }
        int setupCode = 0;
        auto const step = [&setupCode](int code)
        {
            if (setupCode == 0)
                setupCode = code;
        };
        if (options.InputEndsWithParent)
            step(posix_spawn_file_actions_adddup2(setup.Actions(), inputRead.Get(), STDIN_FILENO));
        else
            step(posix_spawn_file_actions_addopen(setup.Actions(), STDIN_FILENO, "/dev/null", O_RDONLY, 0));
        step(posix_spawn_file_actions_adddup2(setup.Actions(), outputWrite.Get(), STDOUT_FILENO));
        step(posix_spawn_file_actions_adddup2(setup.Actions(), errorWrite.Get(), STDERR_FILENO));
#ifdef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
            step(posix_spawn_file_actions_addchdir_np(setup.Actions(), workingDirectory.c_str()));
#endif
#ifdef AMBROSE_SPAWN_CLOSEFROM
        step(posix_spawn_file_actions_addclosefrom_np(setup.Actions(), STDERR_FILENO + 1));
#endif
        int flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
#ifdef __APPLE__
        flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
        step(posix_spawnattr_setflags(setup.Attributes(), static_cast<short>(flags)));
        step(posix_spawnattr_setpgroup(setup.Attributes(), 0));
        sigset_t mask{};
        sigemptyset(&mask);
        step(posix_spawnattr_setsigmask(setup.Attributes(), &mask));
        sigset_t defaults{};
        sigemptyset(&defaults);
        for (int const number : DefaultSignals)
            sigaddset(&defaults, number);
        step(posix_spawnattr_setsigdefault(setup.Attributes(), &defaults));
        if (setupCode != 0)
        {
            result.Error = fmt::format("{} could not be started because its spawn settings could not be prepared: {}", programText, ErrnoMessage(setupCode));
            return;
        }

        std::vector<std::string> strings;
        std::vector<char*> argv = BuildArgv(program, options, strings);
        pid_t id = 0;
        int const spawned = search
            ? posix_spawnp(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ)
            : posix_spawn(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ);
        outputWrite.Reset();
        errorWrite.Reset();
        inputRead.Reset();
        if (spawned != 0)
        {
            result.Error = fmt::format("{} could not be started: {}", programText, ErrnoMessage(spawned));
            return;
        }
        ChildGroup child(id);
        result.Started = true;

        std::array<PipeReader*, 2> const readers{ &output, &errors };
        Supervisor supervisor(options, result);
        bool gaveUp = false;
        std::chrono::milliseconds exitWait = FirstExitWait;
        while (true)
        {
            if (supervisor.Exited() && !output.IsOpen() && !errors.IsOpen())
                break;
            std::array<pollfd, 2> descriptors{};
            std::array<PipeReader*, 2> polled{};
            nfds_t count = 0;
            for (PipeReader* const reader : readers)
            {
                if (!reader->IsOpen())
                    continue;
                descriptors[count] = pollfd{ reader->Get(), POLLIN, 0 };
                polled[count] = reader;
                ++count;
            }
            std::chrono::milliseconds wait = supervisor.NextWait();
            if (count == 0)
            {
                wait = std::min(wait, exitWait);
                exitWait = std::min(exitWait * 2, ChildProcess::PollInterval);
            }
            int const ready = poll(count == 0 ? nullptr : descriptors.data(), count, static_cast<int>(wait.count()));
            if (ready > 0)
            {
                for (nfds_t index = 0; index < count; ++index)
                {
                    if (descriptors[index].revents != 0)
                        polled[index]->Read();
                }
            }
            else if (ready < 0 && errno != EINTR)
            {
                std::this_thread::sleep_for(ChildProcess::PollInterval);
            }
            if (!supervisor.Exited())
            {
                std::string waitError;
                if (child.HasExited(waitError))
                {
                    supervisor.MarkExited();
                    if (!waitError.empty())
                        result.Error = fmt::format("{} could not be waited for: {}", programText, waitError);
                }
            }
            Action const action = supervisor.Next(output.IsOpen() || errors.IsOpen());
            if (action == Action::Terminate)
            {
                child.Signal(SIGTERM);
            }
            else if (action == Action::Kill || action == Action::EndLeftovers)
            {
                child.Signal(SIGKILL);
            }
            else if (action == Action::AbandonOutput)
            {
                output.Abandon();
                errors.Abandon();
            }
            else if (action == Action::GiveUp)
            {
                result.Error = fmt::format("{} did not end after it was killed", programText);
                gaveUp = true;
                break;
            }
        }
        if (gaveUp)
        {
            child.Abandon();
            return;
        }
        child.Signal(SIGKILL);
        int waitError = 0;
        std::optional<int> const status = child.Reap(waitError);
        if (supervisor.Terminated())
            return;
        if (!status)
        {
            if (result.Error.empty())
                result.Error = fmt::format("{} could not be waited for: {}", programText, ErrnoMessage(waitError));
            return;
        }
        int const value = *status;
        if (WIFEXITED(value))
            result.ExitCode = WEXITSTATUS(value);
        else if (WIFSIGNALED(value))
            result.Error = fmt::format("{} was ended by signal {}", programText, WTERMSIG(value));
    }
#endif
}

ChildProcessResult ChildProcess::Run(ChildProcessOptions const& options)
{
    ChildProcessResult result;
    std::string const programText = PathText(options.Program);
    result.Error = CheckOptions(options, programText);
    if (!result.Error.empty())
        return result;
    RunPlatform(options, programText, result);
    return result;
}

ChildProcessResult ChildProcess::StartDetached(ChildProcessOptions const& options)
{
    ChildProcessResult result;
    std::string const programText = PathText(options.Program);
    result.Error = CheckOptions(options, programText);
    if (!result.Error.empty())
        return result;
    StartDetachedPlatform(options, programText, result);
    return result;
}

std::string ChildProcess::QuoteWindowsArgument(std::string_view argument)
{
    if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string_view::npos)
        return std::string(argument);
    std::string quoted;
    quoted.reserve(argument.size() + 2);
    quoted.push_back('"');
    std::size_t backslashes = 0;
    for (char const character : argument)
    {
        if (character == '\\')
        {
            ++backslashes;
            continue;
        }
        quoted.append(character == '"' ? backslashes * 2 + 1 : backslashes, '\\');
        quoted.push_back(character);
        backslashes = 0;
    }
    quoted.append(backslashes * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

void ChildProcess::ExitWhenInputEnds(int exitCode)
{
    std::thread([exitCode]
    {
        std::array<char, InputWatchBytes> buffer{};
#ifdef _WIN32
        HANDLE const input = GetStdHandle(STD_INPUT_HANDLE);
        while (input != nullptr && input != INVALID_HANDLE_VALUE)
        {
            DWORD bytes = 0;
            if (!ReadFile(input, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr) || bytes == 0)
                break;
        }
#else
        while (true)
        {
            ssize_t const bytes = read(STDIN_FILENO, buffer.data(), buffer.size());
            if (bytes > 0 || (bytes < 0 && errno == EINTR))
                continue;
            if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                pollfd descriptor{ STDIN_FILENO, POLLIN, 0 };
                poll(&descriptor, 1, -1);
                continue;
            }
            break;
        }
#endif
        std::_Exit(exitCode);
    }).detach();
}

bool ChildProcessIdentity::Matches(ChildProcessIdentity const& other) const
{
    if (Id != other.Id || StartTime != other.StartTime || BootId != other.BootId)
        return false;
#ifdef _WIN32
    return CompareStringOrdinal(Executable.c_str(), -1, other.Executable.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return Executable == other.Executable;
#endif
}

#ifdef _WIN32
struct ChildProcessHandle::State
{
    ChildProcessIdentity Identity;
    Handle Process;
    Handle Job;
    Handle Input;
    ChildBreakSender SendBreak;
    bool Adopted = false;
    std::mutex Mutex;
    std::optional<ChildExit> Exit;
};

namespace
{
    std::wstring JobName(ChildProcessIdentity const& identity)
    {
        return L"Local\\ProjectAmbrose.Job." + std::to_wstring(identity.Id) + L"." + std::to_wstring(identity.StartTime);
    }

    bool IsRunning(HANDLE process)
    {
        return WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    }

    std::optional<ChildProcessIdentity> ReadIdentity(HANDLE process, DWORD id)
    {
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(process, &created, &exited, &kernel, &user))
            return std::nullopt;
        std::wstring image(MaxCommandLineCharacters, L'\0');
        DWORD size = static_cast<DWORD>(image.size());
        if (!QueryFullProcessImageNameW(process, 0, image.data(), &size))
            return std::nullopt;
        image.resize(size);
        ChildProcessIdentity identity;
        identity.Id = static_cast<int64>(id);
        identity.StartTime = (static_cast<uint64>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
        identity.Executable = std::filesystem::path(image);
        return identity;
    }

    Handle OpenAppendFile(std::filesystem::path const& file, std::string_view what, std::string const& programText, std::string& error)
    {
        if (file.empty())
        {
            Handle discard(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!discard)
                error = fmt::format("{} could not be started because the NUL device could not be opened for its {}: {}", programText, what, SystemMessage(GetLastError()));
            return discard;
        }
        Handle handle(CreateFileW(file.c_str(), FILE_APPEND_DATA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!handle)
            error = fmt::format("{} could not be started because its {} file {} could not be opened: {}", programText, what, PathText(file), SystemMessage(GetLastError()));
        return handle;
    }

    std::unique_ptr<ChildProcessHandle::State> LaunchState(ChildLaunchOptions const& options, ChildProcessOptions const& run, std::string const& programText, std::string& error)
    {
        ChildProcessResult result;
        WindowsCommand command;
        if (!PrepareCommand(run, programText, command, result))
        {
            error = result.Error;
            return nullptr;
        }
        Handle output = OpenAppendFile(options.OutputFile, "output", programText, error);
        if (!output)
            return nullptr;
        Handle errors = OpenAppendFile(options.ErrorFile, "error output", programText, error);
        if (!errors)
            return nullptr;
        Handle input;
        Handle inputHeld;
        if (options.KeepInput)
        {
            HANDLE readEnd = nullptr;
            HANDLE writeEnd = nullptr;
            if (!CreatePipe(&readEnd, &writeEnd, nullptr, 0))
            {
                error = fmt::format("{} could not be started because a pipe for its input could not be created: {}", programText, SystemMessage(GetLastError()));
                return nullptr;
            }
            input = Handle(readEnd);
            inputHeld = Handle(writeEnd);
            DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
            if (!SetNamedPipeHandleState(inputHeld.Get(), &mode, nullptr, nullptr))
            {
                error = fmt::format("{} could not be started because its input pipe could not be made non-blocking: {}", programText, SystemMessage(GetLastError()));
                return nullptr;
            }
        }
        else
        {
            input = Handle(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!input)
            {
                error = fmt::format("{} could not be started because the NUL device could not be opened for its input: {}", programText, SystemMessage(GetLastError()));
                return nullptr;
            }
        }
        std::array<HANDLE, 3> inherited{ input.Get(), output.Get(), errors.Get() };
        for (HANDLE const handle : inherited)
        {
            if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
            {
                error = fmt::format("{} could not be started because a handle could not be made inheritable: {}", programText, SystemMessage(GetLastError()));
                return nullptr;
            }
        }
        AttributeList attributes(inherited);
        if (attributes.Get() == nullptr)
        {
            error = fmt::format("{} could not be started because its handle list could not be built: {}", programText, SystemMessage(attributes.Error()));
            return nullptr;
        }

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.StartupInfo.hStdInput = input.Get();
        startup.StartupInfo.hStdOutput = output.Get();
        startup.StartupInfo.hStdError = errors.Get();
        startup.lpAttributeList = attributes.Get();
        DWORD const creation = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT;
        LPCWSTR const application = command.Search ? nullptr : command.Program.c_str();
        LPCWSTR const directory = command.WorkingDirectory.empty() ? nullptr : command.WorkingDirectory.c_str();
        PROCESS_INFORMATION information{};
        std::wstring commandBuffer = command.CommandBuffer;
        BOOL created = CreateProcessW(application, commandBuffer.data(), nullptr, nullptr, TRUE, creation | CREATE_BREAKAWAY_FROM_JOB, nullptr, directory, &startup.StartupInfo, &information);
        DWORD createError = GetLastError();
        if (!created && createError == ERROR_ACCESS_DENIED)
        {
            commandBuffer = command.CommandBuffer;
            created = CreateProcessW(application, commandBuffer.data(), nullptr, nullptr, TRUE, creation, nullptr, directory, &startup.StartupInfo, &information);
            createError = GetLastError();
        }
        input.Reset();
        output.Reset();
        errors.Reset();
        if (!created)
        {
            error = fmt::format("{} could not be started: {}", programText, SystemMessage(createError));
            return nullptr;
        }
        Handle process(information.hProcess);
        Handle thread(information.hThread);
        std::optional<ChildProcessIdentity> const identity = ReadIdentity(process.Get(), information.dwProcessId);
        if (!identity)
        {
            DWORD const identityError = GetLastError();
            TerminateProcess(process.Get(), 1);
            error = fmt::format("{} could not be started because its identity could not be read: {}", programText, SystemMessage(identityError));
            return nullptr;
        }
        Handle job(CreateJobObjectW(nullptr, JobName(*identity).c_str()));
        if (job)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
            if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) || !AssignProcessToJobObject(job.Get(), process.Get()))
                job.Reset();
        }
        if (job)
        {
            HANDLE kept = nullptr;
            DuplicateHandle(GetCurrentProcess(), job.Get(), process.Get(), &kept, JOB_OBJECT_QUERY, FALSE, 0);
        }
        if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1))
        {
            DWORD const resumeError = GetLastError();
            TerminateProcess(process.Get(), 1);
            error = fmt::format("{} could not be started because its first thread could not be resumed: {}", programText, SystemMessage(resumeError));
            return nullptr;
        }
        auto state = std::make_unique<ChildProcessHandle::State>();
        state->Identity = *identity;
        state->Process = std::move(process);
        state->Job = std::move(job);
        state->Input = std::move(inputHeld);
        state->SendBreak = options.SendBreak;
        return state;
    }

    std::optional<ChildProcessIdentity> DescribeProcess(int64 id)
    {
        if (id <= 0 || id > static_cast<int64>(std::numeric_limits<DWORD>::max()))
            return std::nullopt;
        Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(id)));
        if (!process || !IsRunning(process.Get()))
            return std::nullopt;
        return ReadIdentity(process.Get(), static_cast<DWORD>(id));
    }

    std::unique_ptr<ChildProcessHandle::State> AdoptState(ChildProcessIdentity const& expected, ChildBreakSender sendBreak, std::string& error)
    {
        if (expected.Id <= 0 || expected.Id > static_cast<int64>(std::numeric_limits<DWORD>::max()))
        {
            error = fmt::format("{} is not a process id", expected.Id);
            return nullptr;
        }
        Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(expected.Id)));
        if (!process)
        {
            error = fmt::format("process {} is not running or cannot be opened: {}", expected.Id, SystemMessage(GetLastError()));
            return nullptr;
        }
        if (!IsRunning(process.Get()))
        {
            error = fmt::format("process {} has ended", expected.Id);
            return nullptr;
        }
        std::optional<ChildProcessIdentity> const actual = ReadIdentity(process.Get(), static_cast<DWORD>(expected.Id));
        if (!actual)
        {
            error = fmt::format("the identity of process {} could not be read: {}", expected.Id, SystemMessage(GetLastError()));
            return nullptr;
        }
        if (!actual->Matches(expected))
        {
            error = fmt::format("process {} is {} started at another time, not the program that was recorded", expected.Id, PathText(actual->Executable));
            return nullptr;
        }
        Handle job(OpenJobObjectW(JOB_OBJECT_TERMINATE | JOB_OBJECT_QUERY, FALSE, JobName(expected).c_str()));
        if (job)
        {
            BOOL inJob = FALSE;
            if (!IsProcessInJob(process.Get(), job.Get(), &inJob) || !inJob)
                job.Reset();
        }
        auto state = std::make_unique<ChildProcessHandle::State>();
        state->Identity = *actual;
        state->Process = std::move(process);
        state->Job = std::move(job);
        state->SendBreak = std::move(sendBreak);
        state->Adopted = true;
        return state;
    }
}

bool ChildProcessHandle::WaitForExit(std::chrono::milliseconds timeout)
{
    State& state = *_state;
    {
        std::lock_guard<std::mutex> const lock(state.Mutex);
        if (state.Exit)
            return true;
    }
    DWORD const wait = static_cast<DWORD>(std::clamp<int64>(timeout.count(), 0, static_cast<int64>(INFINITE) - 1));
    if (WaitForSingleObject(state.Process.Get(), wait) != WAIT_OBJECT_0)
        return false;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    if (!state.Exit)
    {
        ChildExit exit;
        DWORD code = 0;
        if (GetExitCodeProcess(state.Process.Get(), &code))
            exit.Code = static_cast<int64>(code);
        state.Exit = exit;
    }
    return true;
}

bool ChildProcessHandle::WriteInput(std::string_view text, std::string& error)
{
    State& state = *_state;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    if (!state.Input)
    {
        error = "this process was not started with an input pipe";
        return false;
    }
    if (state.Exit)
    {
        error = "the process has ended";
        return false;
    }
    DWORD written = 0;
    if (!WriteFile(state.Input.Get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr))
    {
        error = fmt::format("its input could not be written: {}", SystemMessage(GetLastError()));
        return false;
    }
    if (written != text.size())
    {
        error = "its input is full, so the process is not reading it";
        return false;
    }
    return true;
}

bool ChildProcessHandle::Interrupt(std::string& error)
{
    State& state = *_state;
    ChildBreakSender sendBreak;
    {
        std::lock_guard<std::mutex> const lock(state.Mutex);
        if (state.Exit || !IsRunning(state.Process.Get()))
        {
            error = "the process has ended";
            return false;
        }
        sendBreak = state.SendBreak;
    }
    if (!sendBreak)
    {
        error = "no helper was given to send Ctrl+Break to its console";
        return false;
    }
    return sendBreak(state.Identity.Id, error);
}

bool ChildProcessHandle::EndTree(std::string& error)
{
    State& state = *_state;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    if (state.Job)
    {
        if (TerminateJobObject(state.Job.Get(), 1))
            return true;
        error = fmt::format("its job could not be ended: {}", SystemMessage(GetLastError()));
        return false;
    }
    if (!IsRunning(state.Process.Get()))
        return true;
    if (TerminateProcess(state.Process.Get(), 1))
        return true;
    error = fmt::format("it could not be ended: {}", SystemMessage(GetLastError()));
    return false;
}

bool ChildProcess::SendConsoleBreak(int64 group, std::string& error)
{
    if (group <= 0 || group > static_cast<int64>(std::numeric_limits<DWORD>::max()))
    {
        error = fmt::format("{} is not a process group", group);
        return false;
    }
    FreeConsole();
    if (!AttachConsole(static_cast<DWORD>(group)))
    {
        error = fmt::format("the console of process {} could not be attached: {}", group, SystemMessage(GetLastError()));
        return false;
    }
    SetConsoleCtrlHandler(nullptr, TRUE);
    bool const sent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(group)) != 0;
    if (!sent)
        error = fmt::format("Ctrl+Break could not be sent to process group {}: {}", group, SystemMessage(GetLastError()));
    FreeConsole();
    return sent;
}
#else
struct ChildProcessHandle::State
{
    ChildProcessIdentity Identity;
    Descriptor Input;
    Descriptor Watch;
    ChildBreakSender SendBreak;
    bool Adopted = false;
    std::mutex Mutex;
    std::optional<ChildExit> Exit;
};

namespace
{
    constexpr std::chrono::milliseconds ExitPollInterval{ 20 };
    constexpr std::string_view DeletedSuffix = " (deleted)";

    std::string ReadFirstLine(std::string const& file)
    {
        std::ifstream stream(file);
        std::string line;
        std::getline(stream, line);
        return line;
    }

    std::string ReadBootId()
    {
#ifdef __linux__
        return std::string(Ambrose::Trim(ReadFirstLine("/proc/sys/kernel/random/boot_id")));
#else
        return std::string();
#endif
    }

    std::optional<ChildProcessIdentity> DescribeProcess(int64 id)
    {
#ifdef __linux__
        if (id <= 0 || id > static_cast<int64>(std::numeric_limits<pid_t>::max()))
            return std::nullopt;
        std::string const folder = "/proc/" + std::to_string(id);
        std::string const stat = ReadFirstLine(folder + "/stat");
        std::size_t const close = stat.rfind(')');
        if (close == std::string::npos)
            return std::nullopt;
        std::string_view rest = std::string_view(stat).substr(close + 1);
        std::vector<std::string_view> fields;
        while (!rest.empty())
        {
            std::size_t const start = rest.find_first_not_of(' ');
            if (start == std::string_view::npos)
                break;
            rest.remove_prefix(start);
            std::size_t const end = rest.find(' ');
            fields.push_back(rest.substr(0, end));
            rest.remove_prefix(end == std::string_view::npos ? rest.size() : end);
        }
        constexpr std::size_t StateField = 0;
        constexpr std::size_t StartTimeField = 19;
        if (fields.size() <= StartTimeField || fields[StateField] == "Z" || fields[StateField] == "X")
            return std::nullopt;
        uint64 startTime = 0;
        std::string_view const text = fields[StartTimeField];
        auto const [end, parsed] = std::from_chars(text.data(), text.data() + text.size(), startTime);
        if (parsed != std::errc() || end != text.data() + text.size())
            return std::nullopt;
        std::error_code linkError;
        std::string executable = std::filesystem::read_symlink(folder + "/exe", linkError).native();
        if (linkError)
            return std::nullopt;
        if (executable.size() > DeletedSuffix.size() && std::string_view(executable).substr(executable.size() - DeletedSuffix.size()) == DeletedSuffix)
            executable.resize(executable.size() - DeletedSuffix.size());
        ChildProcessIdentity identity;
        identity.Id = id;
        identity.StartTime = startTime;
        identity.BootId = ReadBootId();
        identity.Executable = std::filesystem::path(executable);
        return identity;
#else
        (void)id;
        return std::nullopt;
#endif
    }

    Descriptor WatchProcess([[maybe_unused]] pid_t id)
    {
#if defined(__linux__) && defined(SYS_pidfd_open)
        long const descriptor = syscall(SYS_pidfd_open, id, 0);
        return descriptor < 0 ? Descriptor() : Descriptor(static_cast<int>(descriptor));
#else
        return Descriptor();
#endif
    }

    int OpenAppendFile(std::filesystem::path const& file, Descriptor& descriptor)
    {
        int const opened = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0600);
        if (opened < 0)
            return errno;
        descriptor = Descriptor(opened);
        if (descriptor.Get() > STDERR_FILENO)
            return 0;
        int const moved = fcntl(descriptor.Get(), F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (moved < 0)
            return errno;
        descriptor = Descriptor(moved);
        return 0;
    }

    std::unique_ptr<ChildProcessHandle::State> LaunchState(ChildLaunchOptions const& options, ChildProcessOptions const&, std::string const& programText, std::string& error)
    {
        ChildProcessResult result;
        std::filesystem::path program = options.Program;
        bool const search = program.native().find('/') == std::string::npos;
        if (!search && !MakeAbsolute(program, "program", result))
        {
            error = result.Error;
            return nullptr;
        }
        std::filesystem::path workingDirectory = options.WorkingDirectory;
        if (!workingDirectory.empty() && !MakeAbsolute(workingDirectory, "working directory", result))
        {
            error = result.Error;
            return nullptr;
        }
#ifndef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
        {
            error = fmt::format("{} could not be started because this system cannot start a program in another working directory", programText);
            return nullptr;
        }
#endif
        if (int const code = KeepChildrenWaitable(); code != 0)
        {
            error = fmt::format("{} could not be started because SIGCHLD would reap it before its exit code is read, and that could not be changed: {}", programText, ErrnoMessage(code));
            return nullptr;
        }
        Descriptor output;
        Descriptor errors;
        if (!options.OutputFile.empty())
        {
            if (int const code = OpenAppendFile(options.OutputFile, output); code != 0)
            {
                error = fmt::format("{} could not be started because its output file {} could not be opened: {}", programText, PathText(options.OutputFile), ErrnoMessage(code));
                return nullptr;
            }
        }
        if (!options.ErrorFile.empty())
        {
            if (int const code = OpenAppendFile(options.ErrorFile, errors); code != 0)
            {
                error = fmt::format("{} could not be started because its error output file {} could not be opened: {}", programText, PathText(options.ErrorFile), ErrnoMessage(code));
                return nullptr;
            }
        }
        Descriptor inputRead;
        Descriptor inputHeld;
        if (options.KeepInput)
        {
            if (int const code = MakePipe(inputRead, inputHeld, false); code != 0)
            {
                error = fmt::format("{} could not be started because a pipe for its input could not be created: {}", programText, ErrnoMessage(code));
                return nullptr;
            }
            int const flags = fcntl(inputHeld.Get(), F_GETFL);
            if (flags < 0 || fcntl(inputHeld.Get(), F_SETFL, flags | O_NONBLOCK) != 0)
            {
                error = fmt::format("{} could not be started because its input pipe could not be made non-blocking: {}", programText, ErrnoMessage(errno));
                return nullptr;
            }
        }

        SpawnSetup setup;
        if (!setup.Ready())
        {
            error = fmt::format("{} could not be started because its spawn settings could not be prepared", programText);
            return nullptr;
        }
        int setupCode = 0;
        auto const step = [&setupCode](int code)
        {
            if (setupCode == 0)
                setupCode = code;
        };
        if (options.KeepInput)
            step(posix_spawn_file_actions_adddup2(setup.Actions(), inputRead.Get(), STDIN_FILENO));
        else
            step(posix_spawn_file_actions_addopen(setup.Actions(), STDIN_FILENO, "/dev/null", O_RDONLY, 0));
        if (output)
            step(posix_spawn_file_actions_adddup2(setup.Actions(), output.Get(), STDOUT_FILENO));
        else
            step(posix_spawn_file_actions_addopen(setup.Actions(), STDOUT_FILENO, "/dev/null", O_WRONLY, 0));
        if (errors)
            step(posix_spawn_file_actions_adddup2(setup.Actions(), errors.Get(), STDERR_FILENO));
        else
            step(posix_spawn_file_actions_addopen(setup.Actions(), STDERR_FILENO, "/dev/null", O_WRONLY, 0));
#ifdef AMBROSE_SPAWN_CHDIR
        if (!workingDirectory.empty())
            step(posix_spawn_file_actions_addchdir_np(setup.Actions(), workingDirectory.c_str()));
#endif
#ifdef AMBROSE_SPAWN_CLOSEFROM
        step(posix_spawn_file_actions_addclosefrom_np(setup.Actions(), STDERR_FILENO + 1));
#endif
        int flags = POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
#ifdef POSIX_SPAWN_SETSID
        flags |= POSIX_SPAWN_SETSID;
#else
        flags |= POSIX_SPAWN_SETPGROUP;
        step(posix_spawnattr_setpgroup(setup.Attributes(), 0));
#endif
#ifdef __APPLE__
        flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
        step(posix_spawnattr_setflags(setup.Attributes(), static_cast<short>(flags)));
        sigset_t mask{};
        sigemptyset(&mask);
        step(posix_spawnattr_setsigmask(setup.Attributes(), &mask));
        sigset_t defaults{};
        sigemptyset(&defaults);
        for (int const number : DefaultSignals)
            sigaddset(&defaults, number);
        step(posix_spawnattr_setsigdefault(setup.Attributes(), &defaults));
        if (setupCode != 0)
        {
            error = fmt::format("{} could not be started because its spawn settings could not be prepared: {}", programText, ErrnoMessage(setupCode));
            return nullptr;
        }

        ChildProcessOptions arguments;
        arguments.Arguments = options.Arguments;
        std::vector<std::string> strings;
        std::vector<char*> argv = BuildArgv(program, arguments, strings);
        pid_t id = 0;
        int const spawned = search
            ? posix_spawnp(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ)
            : posix_spawn(&id, program.c_str(), setup.Actions(), setup.Attributes(), argv.data(), environ);
        inputRead.Reset();
        output.Reset();
        errors.Reset();
        if (spawned != 0)
        {
            error = fmt::format("{} could not be started: {}", programText, ErrnoMessage(spawned));
            return nullptr;
        }
        auto state = std::make_unique<ChildProcessHandle::State>();
        std::optional<ChildProcessIdentity> identity = DescribeProcess(id);
        if (!identity)
        {
            identity = ChildProcessIdentity{};
            identity->Id = id;
            identity->BootId = ReadBootId();
            identity->Executable = program;
        }
        state->Identity = std::move(*identity);
        state->Input = std::move(inputHeld);
        state->Watch = WatchProcess(id);
        state->SendBreak = options.SendBreak;
        return state;
    }

    std::unique_ptr<ChildProcessHandle::State> AdoptState(ChildProcessIdentity const& expected, ChildBreakSender sendBreak, std::string& error)
    {
        std::optional<ChildProcessIdentity> const actual = DescribeProcess(expected.Id);
        if (!actual)
        {
            error = fmt::format("process {} is not running or cannot be read", expected.Id);
            return nullptr;
        }
        if (!actual->Matches(expected))
        {
            error = fmt::format("process {} is {} started at another time, not the program that was recorded", expected.Id, PathText(actual->Executable));
            return nullptr;
        }
        if (getpgid(static_cast<pid_t>(expected.Id)) != static_cast<pid_t>(expected.Id))
        {
            error = fmt::format("process {} no longer leads its own process group", expected.Id);
            return nullptr;
        }
        auto state = std::make_unique<ChildProcessHandle::State>();
        state->Identity = *actual;
        state->Watch = WatchProcess(static_cast<pid_t>(expected.Id));
        state->SendBreak = std::move(sendBreak);
        state->Adopted = true;
        return state;
    }

    bool StillRunning(ChildProcessIdentity const& identity)
    {
        std::optional<ChildProcessIdentity> const current = DescribeProcess(identity.Id);
        return current && current->Matches(identity);
    }

    bool SignalGroup(ChildProcessHandle::State& state, int number, std::string& error)
    {
        if (state.Exit || (state.Adopted && !StillRunning(state.Identity)))
        {
            error = "the process has ended";
            return false;
        }
        pid_t const id = static_cast<pid_t>(state.Identity.Id);
        bool const group = kill(-id, number) == 0;
        bool const leader = kill(id, number) == 0;
        if (group || leader)
            return true;
        error = fmt::format("signal {} could not be sent: {}", number, ErrnoMessage(errno));
        return false;
    }
}

bool ChildProcessHandle::WaitForExit(std::chrono::milliseconds timeout)
{
    State& state = *_state;
    Clock::time_point const deadline = Clock::now() + timeout;
    while (true)
    {
        {
            std::lock_guard<std::mutex> const lock(state.Mutex);
            if (state.Exit)
                return true;
            if (state.Adopted)
            {
                if (!StillRunning(state.Identity))
                {
                    state.Exit = ChildExit{};
                    return true;
                }
            }
            else
            {
                int status = 0;
                pid_t const reaped = waitpid(static_cast<pid_t>(state.Identity.Id), &status, WNOHANG);
                if (reaped == static_cast<pid_t>(state.Identity.Id))
                {
                    ChildExit exit;
                    if (WIFEXITED(status))
                        exit.Code = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status))
                        exit.Signal = WTERMSIG(status);
                    state.Exit = exit;
                    return true;
                }
                if (reaped < 0 && errno != EINTR)
                {
                    state.Exit = ChildExit{};
                    return true;
                }
            }
        }
        Clock::time_point const now = Clock::now();
        if (now >= deadline)
            return false;
        std::chrono::milliseconds wait = WaitUntil(now, deadline);
        if (state.Watch)
        {
            pollfd descriptor{ state.Watch.Get(), POLLIN, 0 };
            poll(&descriptor, 1, static_cast<int>(std::min<int64>(wait.count(), std::numeric_limits<int>::max())));
        }
        else
        {
            std::this_thread::sleep_for(std::min(wait, ExitPollInterval));
        }
    }
}

bool ChildProcessHandle::WriteInput(std::string_view text, std::string& error)
{
    State& state = *_state;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    if (!state.Input)
    {
        error = "this process was not started with an input pipe";
        return false;
    }
    if (state.Exit)
    {
        error = "the process has ended";
        return false;
    }
    sigset_t pipeSignal{};
    sigemptyset(&pipeSignal);
    sigaddset(&pipeSignal, SIGPIPE);
    sigset_t previous{};
    pthread_sigmask(SIG_BLOCK, &pipeSignal, &previous);
    int writeError = 0;
    while (!text.empty())
    {
        ssize_t const written = write(state.Input.Get(), text.data(), text.size());
        if (written > 0)
        {
            text.remove_prefix(static_cast<std::size_t>(written));
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        writeError = written < 0 ? errno : EAGAIN;
        break;
    }
    if (writeError == EPIPE)
    {
        timespec const immediately{ 0, 0 };
        sigtimedwait(&pipeSignal, nullptr, &immediately);
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    if (writeError == 0)
        return true;
    if (writeError == EAGAIN || writeError == EWOULDBLOCK)
        error = "its input is full, so the process is not reading it";
    else if (writeError == EPIPE)
        error = "it has closed its input";
    else
        error = fmt::format("its input could not be written: {}", ErrnoMessage(writeError));
    return false;
}

bool ChildProcessHandle::Interrupt(std::string& error)
{
    State& state = *_state;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    return SignalGroup(state, SIGTERM, error);
}

bool ChildProcessHandle::EndTree(std::string& error)
{
    State& state = *_state;
    std::lock_guard<std::mutex> const lock(state.Mutex);
    if (state.Exit)
        return true;
    return SignalGroup(state, SIGKILL, error);
}

bool ChildProcess::SendConsoleBreak(int64, std::string& error)
{
    error = "Ctrl+Break is a Windows console event; POSIX systems stop a process group with SIGTERM";
    return false;
}
#endif

ChildProcessHandle::ChildProcessHandle() noexcept = default;
ChildProcessHandle::~ChildProcessHandle() = default;
ChildProcessHandle::ChildProcessHandle(ChildProcessHandle&& other) noexcept = default;
ChildProcessHandle& ChildProcessHandle::operator=(ChildProcessHandle&& other) noexcept = default;

ChildProcessHandle::ChildProcessHandle(std::unique_ptr<State> state) noexcept : _state(std::move(state))
{
}

ChildProcessHandle ChildProcessHandle::Launch(ChildLaunchOptions const& options, std::string& error)
{
    ChildProcessOptions run;
    run.Program = options.Program;
    run.Arguments = options.Arguments;
    run.WorkingDirectory = options.WorkingDirectory;
    std::string const programText = PathText(options.Program);
    error = CheckOptions(run, programText);
    if (!error.empty())
        return ChildProcessHandle();
    return ChildProcessHandle(LaunchState(options, run, programText, error));
}

ChildProcessHandle ChildProcessHandle::Adopt(ChildProcessIdentity const& identity, ChildBreakSender sendBreak, std::string& error)
{
    return ChildProcessHandle(AdoptState(identity, std::move(sendBreak), error));
}

std::optional<ChildProcessIdentity> ChildProcessHandle::Describe(int64 id)
{
    return DescribeProcess(id);
}

ChildProcessIdentity const& ChildProcessHandle::GetIdentity() const
{
    return _state->Identity;
}

bool ChildProcessHandle::IsAdopted() const
{
    return _state->Adopted;
}

bool ChildProcessHandle::HasInput() const
{
    std::lock_guard<std::mutex> const lock(_state->Mutex);
    return static_cast<bool>(_state->Input);
}

std::optional<ChildExit> ChildProcessHandle::GetExit() const
{
    std::lock_guard<std::mutex> const lock(_state->Mutex);
    return _state->Exit;
}
