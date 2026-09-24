/*
 * Project Ambrose by Imjustchico
 * Program the ChildProcess and supervisor tests run, carrying out the commands in its arguments in order, and when its first argument is --config, reading more from that file one argument per line the way the supervisor starts an app: echo and err print a line to standard output or error, exit ends with a code, sleep waits milliseconds, args prints every later argument on its own line, long prints a line of that many bytes, unfinished prints that many bytes with no newline, pad prints that many bytes and then some text as a line, lines prints that many numbered lines, hex writes bytes given in hex as a line, touch writes an empty file at that path, so a run with no pipes can still be seen, partial writes text with no newline, crlf ends a line with CRLF, stdin prints whether input is already at its end, input prints without waiting whether input is open, at its end or holding data, cwd prints the working directory, ignore-term ignores SIGTERM, exit-when-input-ends calls ChildProcess::ExitWhenInputEnds with a code, spawn-sleeper starts a copy of itself that sleeps for a minute and prints that copy's process id, spawn-input-watcher starts a copy of itself in a process group of its own, sharing its input and discarding its output, that ends once that input ends or else sleeps for a minute, and prints that copy's process id, wait-for-stop prints waiting and runs until a shutdown line arrives on its input or SIGINT, SIGTERM or SIGBREAK does, then says which and exits 0, the way a server stops, and console-break sends Ctrl+Break to a process group through ChildProcess::SendConsoleBreak, exiting 1 with the reason when it cannot.
 */

#include "ChildProcess.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/types.h>
#include <unistd.h>

extern char** environ;
#endif

namespace
{
    volatile std::sig_atomic_t stopSignal = 0;

    void OnStopSignal(int number)
    {
        stopSignal = number;
    }

#ifdef _WIN32
    std::string ToUtf8(std::wstring_view text)
    {
        if (text.empty())
            return std::string();
        int const bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
            return std::string();
        std::string utf8(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), bytes, nullptr, nullptr);
        return utf8;
    }
#endif

    std::vector<std::string> Arguments([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
    {
#ifdef _WIN32
        std::vector<std::string> arguments;
        int count = 0;
        LPWSTR* const wide = CommandLineToArgvW(GetCommandLineW(), &count);
        if (wide == nullptr)
            return arguments;
        for (int index = 0; index < count; ++index)
            arguments.push_back(ToUtf8(wide[index]));
        LocalFree(wide);
        return arguments;
#else
        return std::vector<std::string>(argv, argv + argc);
#endif
    }

    void Write(std::FILE* stream, std::string_view text)
    {
        std::fwrite(text.data(), 1, text.size(), stream);
        std::fflush(stream);
    }

    std::optional<long long> Number(std::string_view text)
    {
        long long value = 0;
        auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (text.empty() || error != std::errc() || end != text.data() + text.size() || value < 0)
            return std::nullopt;
        return value;
    }

    std::optional<std::string> FromHex(std::string_view text)
    {
        if (text.size() % 2 != 0)
            return std::nullopt;
        std::string bytes;
        for (std::size_t index = 0; index < text.size(); index += 2)
        {
            unsigned value = 0;
            auto const [end, error] = std::from_chars(text.data() + index, text.data() + index + 2, value, 16);
            if (error != std::errc() || end != text.data() + index + 2)
                return std::nullopt;
            bytes.push_back(static_cast<char>(value));
        }
        return bytes;
    }

    std::string InputState()
    {
#ifdef _WIN32
        HANDLE const input = GetStdHandle(STD_INPUT_HANDLE);
        if (GetFileType(input) == FILE_TYPE_PIPE)
        {
            DWORD available = 0;
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr))
                return "eof";
            return available == 0 ? "open" : "data";
        }
        char byte = 0;
        DWORD got = 0;
        return ReadFile(input, &byte, 1, &got, nullptr) && got > 0 ? "data" : "eof";
#else
        pollfd descriptor{ STDIN_FILENO, POLLIN, 0 };
        if (poll(&descriptor, 1, 0) == 0)
            return "open";
        char byte = 0;
        return read(STDIN_FILENO, &byte, 1) > 0 ? "data" : "eof";
#endif
    }

    long long SpawnCopy([[maybe_unused]] std::string const& self, std::vector<std::string> const& commands, bool ownGroup)
    {
#ifdef _WIN32
        std::wstring path(32768, L'\0');
        DWORD const length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
            return -1;
        path.resize(length);
        std::wstring commandLine = L"\"" + path + L"\"";
        for (std::string const& command : commands)
            commandLine += L" " + std::wstring(command.begin(), command.end());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        HANDLE discard = INVALID_HANDLE_VALUE;
        if (ownGroup)
        {
            SECURITY_ATTRIBUTES inherit{ sizeof(inherit), nullptr, TRUE };
            discard = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (discard == INVALID_HANDLE_VALUE)
                return -1;
            SetHandleInformation(startup.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            SetHandleInformation(startup.hStdOutput, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(startup.hStdError, HANDLE_FLAG_INHERIT, 0);
            startup.hStdOutput = discard;
            startup.hStdError = discard;
        }
        PROCESS_INFORMATION information{};
        BOOL const created = CreateProcessW(path.c_str(), commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &information);
        if (discard != INVALID_HANDLE_VALUE)
            CloseHandle(discard);
        if (!created)
            return -1;
        CloseHandle(information.hThread);
        CloseHandle(information.hProcess);
        return static_cast<long long>(information.dwProcessId);
#else
#ifdef __linux__
        std::string const program = "/proc/self/exe";
#else
        std::string const program = self;
#endif
        std::vector<std::string> strings{ program };
        strings.insert(strings.end(), commands.begin(), commands.end());
        std::vector<char*> argv;
        for (std::string& text : strings)
            argv.push_back(text.data());
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions) != 0)
            return -1;
        posix_spawnattr_t attributes;
        if (posix_spawnattr_init(&attributes) != 0)
        {
            posix_spawn_file_actions_destroy(&actions);
            return -1;
        }
        int prepared = 0;
        if (ownGroup)
        {
            prepared = prepared != 0 ? prepared : posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
            prepared = prepared != 0 ? prepared : posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
            prepared = prepared != 0 ? prepared : posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
            prepared = prepared != 0 ? prepared : posix_spawnattr_setpgroup(&attributes, 0);
        }
        pid_t id = 0;
        int const spawned = prepared != 0 ? prepared : posix_spawn(&id, program.c_str(), &actions, &attributes, argv.data(), environ);
        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);
        if (spawned != 0)
            return -1;
        return static_cast<long long>(id);
#endif
    }
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    std::vector<std::string> arguments = Arguments(argc, argv);
    if (arguments.size() >= 3 && arguments[1] == "--config")
    {
        std::ifstream script(std::filesystem::path(std::u8string(arguments[2].begin(), arguments[2].end())), std::ios::binary);
        std::vector<std::string> expanded{ arguments[0] };
        std::string line;
        while (std::getline(script, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            expanded.push_back(line);
        }
        expanded.insert(expanded.end(), arguments.begin() + 3, arguments.end());
        arguments = std::move(expanded);
    }
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        std::string const command = arguments[index];
        if (command == "args")
        {
            std::string text;
            for (std::size_t later = index + 1; later < arguments.size(); ++later)
                text.append(arguments[later]).push_back('\n');
            Write(stdout, text);
            return 0;
        }
        if (command == "stdin")
        {
            char byte = 0;
            std::size_t const got = std::fread(&byte, 1, 1, stdin);
            Write(stdout, got == 0 && std::feof(stdin) ? "eof\n" : "data\n");
            continue;
        }
        if (command == "cwd")
        {
#ifdef _WIN32
            Write(stdout, ToUtf8(std::filesystem::current_path().native()) + "\n");
#else
            Write(stdout, std::filesystem::current_path().native() + "\n");
#endif
            continue;
        }
        if (command == "ignore-term")
        {
#ifndef _WIN32
            std::signal(SIGTERM, SIG_IGN);
#endif
            continue;
        }
        if (command == "input")
        {
            Write(stdout, InputState() + "\n");
            continue;
        }
        if (command == "wait-for-stop")
        {
            std::signal(SIGINT, OnStopSignal);
            std::signal(SIGTERM, OnStopSignal);
#ifdef SIGBREAK
            std::signal(SIGBREAK, OnStopSignal);
#endif
            static std::atomic<bool> shutdownLine{ false };
            std::thread([]
            {
                std::array<char, 256> line{};
                while (std::fgets(line.data(), static_cast<int>(line.size()), stdin) != nullptr)
                {
                    std::string_view text(line.data());
                    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                        text.remove_suffix(1);
                    if (text == "shutdown")
                    {
                        shutdownLine = true;
                        return;
                    }
                }
            }).detach();
            Write(stdout, "waiting\n");
            while (stopSignal == 0 && !shutdownLine.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Write(stdout, stopSignal != 0 ? "stopped by signal\n" : "stopped by shutdown\n");
            return 0;
        }
        if (command == "spawn-sleeper" || command == "spawn-input-watcher")
        {
            bool const watcher = command == "spawn-input-watcher";
            std::vector<std::string> const commands = watcher ? std::vector<std::string>{ "exit-when-input-ends", "0", "sleep", "60000" } : std::vector<std::string>{ "sleep", "60000" };
            long long const id = SpawnCopy(arguments.front(), commands, watcher);
            if (id < 0)
            {
                Write(stderr, "the copy could not be started\n");
                return 3;
            }
            Write(stdout, std::to_string(id) + "\n");
            continue;
        }
        if (command == "pad" && index + 2 < arguments.size())
        {
            std::optional<long long> const padding = Number(arguments[index + 1]);
            if (!padding)
            {
                Write(stderr, "pad needs a number that is not negative\n");
                return 2;
            }
            Write(stdout, std::string(static_cast<std::size_t>(*padding), 'x') + arguments[index + 2] + "\n");
            index += 2;
            continue;
        }
        if (index + 1 >= arguments.size())
        {
            Write(stderr, command + " needs a value\n");
            return 2;
        }
        std::string const value = arguments[++index];
        if (command == "touch")
        {
            std::ofstream stream(std::filesystem::path(std::u8string(value.begin(), value.end())), std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                Write(stderr, "touch could not write " + value + "\n");
                return 2;
            }
            continue;
        }
        if (command == "echo")
        {
            Write(stdout, value + "\n");
            continue;
        }
        if (command == "err")
        {
            Write(stderr, value + "\n");
            continue;
        }
        if (command == "partial")
        {
            Write(stdout, value);
            continue;
        }
        if (command == "crlf")
        {
            Write(stdout, value + "\r\n");
            continue;
        }
        if (command == "hex")
        {
            std::optional<std::string> const bytes = FromHex(value);
            if (!bytes)
            {
                Write(stderr, "hex needs pairs of hex digits\n");
                return 2;
            }
            Write(stdout, *bytes + "\n");
            continue;
        }
        std::optional<long long> const number = Number(value);
        if (!number)
        {
            Write(stderr, command + " needs a number that is not negative\n");
            return 2;
        }
        if (command == "exit")
            return static_cast<int>(*number);
        if (command == "sleep")
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(*number));
        }
        else if (command == "exit-when-input-ends")
        {
            ChildProcess::ExitWhenInputEnds(static_cast<int>(*number));
        }
        else if (command == "console-break")
        {
            std::string error;
            if (!ChildProcess::SendConsoleBreak(static_cast<int64>(*number), error))
            {
                Write(stderr, error + "\n");
                return 1;
            }
        }
        else if (command == "long")
        {
            Write(stdout, std::string(static_cast<std::size_t>(*number), 'x') + "\n");
        }
        else if (command == "unfinished")
        {
            Write(stdout, std::string(static_cast<std::size_t>(*number), 'x'));
        }
        else if (command == "lines")
        {
            std::string text;
            for (long long line = 0; line < *number; ++line)
                text.append(std::to_string(line)).push_back('\n');
            Write(stdout, text);
        }
        else
        {
            Write(stderr, "unknown command " + command + "\n");
            return 2;
        }
    }
    return 0;
}
