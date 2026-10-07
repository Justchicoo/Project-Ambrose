/*
 * Project Ambrose by Imjustchico
 * Unit test entry point that initializes GoogleTest and GoogleMock and runs every test, having first told the Microsoft runtime, and in a debug build its debug runtime, to report a failed assertion or a corrupted heap on standard error rather than in a window, because a window waits for somebody to click it and there is nobody there: a run that would have failed in a second otherwise hangs until it is killed, and the reason it failed is on a screen no log keeps. With AMBROSE_TEST_DB set and AMBROSE_TEST_DB_WSL naming the WSL distribution that serves it, a Windows test process keeps that distribution attached until it ends, through a wsl.exe in a job that closes with the process, because WSL ends a distribution, and the MariaDB inside it, once no wsl.exe is attached, which took the database away part way through a run and left every test after that waiting out its own timeout.
 */

#include "Environment.h"

#include <gmock/gmock.h>

#include <filesystem>
#include <optional>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#ifdef _WIN32
#include <windows.h>

namespace
{
    void HoldWslDatabase()
    {
        std::optional<std::string> const database = Ambrose::GetEnv("AMBROSE_TEST_DB");
        std::optional<std::string> const distribution = Ambrose::GetEnv("AMBROSE_TEST_DB_WSL");
        if (!database || database->empty() || !distribution || distribution->empty())
            return;
        HANDLE const job = CreateJobObjectW(nullptr, nullptr);
        if (!job)
            return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        std::u8string const name(distribution->begin(), distribution->end());
        std::wstring command = L"wsl.exe -d \"" + std::filesystem::path(name).wstring() + L"\" --exec sleep infinity";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process))
            return;
        AssignProcessToJobObject(job, process.hProcess);
        ResumeThread(process.hThread);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}
#endif

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
#endif
#if defined(_MSC_VER) && defined(_DEBUG)
    for (int const report : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
    }
#endif
#ifdef _WIN32
    HoldWslDatabase();
#endif
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
