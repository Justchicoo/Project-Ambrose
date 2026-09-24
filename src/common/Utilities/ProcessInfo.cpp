/*
 * Project Ambrose by Imjustchico
 * Reads a process from GetProcessMemoryInfo, GetProcessTimes, GetProcessHandleCount, GetProcessIoCounters and a Toolhelp thread snapshot on Windows, and from /proc/<pid>/statm, stat, fd and io elsewhere. A figure the operating system will not give is left at zero rather than failing the whole snapshot, because a process running as another user answers some of these and not others, and an operator is better served by the memory and processor time than by nothing at all. A share of the processor is worked out from two snapshots and the time between them, clamped to what the cores available could have done, since a reading taken while the clock moved backwards or not at all would otherwise report a number no machine could produce.
 */

#include "ProcessInfo.h"

#include <algorithm>

#ifdef _WIN32
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#endif

namespace Ambrose
{
    std::optional<ProcessSnapshot> ProcessInfo::Snapshot()
    {
        return SnapshotOf(CurrentProcessId());
    }

    double ProcessInfo::CpuShare(ProcessSnapshot const& earlier, ProcessSnapshot const& later, uint64 elapsedMicroseconds, uint32 cores)
    {
        if (elapsedMicroseconds == 0 || later.CpuMicroseconds < earlier.CpuMicroseconds)
            return 0.0;
        uint64 const used = later.CpuMicroseconds - earlier.CpuMicroseconds;
        double const share = static_cast<double>(used) * 100.0 / static_cast<double>(elapsedMicroseconds);
        double const ceiling = static_cast<double>(cores == 0 ? 1 : cores) * 100.0;
        return std::clamp(share, 0.0, ceiling);
    }

#ifdef _WIN32
    uint32 ProcessInfo::CurrentProcessId()
    {
        return static_cast<uint32>(GetCurrentProcessId());
    }

    uint32 ProcessInfo::CoreCount()
    {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return info.dwNumberOfProcessors == 0 ? 1 : static_cast<uint32>(info.dwNumberOfProcessors);
    }

    namespace
    {
        uint64 Microseconds(FILETIME const& time)
        {
            ULARGE_INTEGER value{};
            value.LowPart = time.dwLowDateTime;
            value.HighPart = time.dwHighDateTime;
            return value.QuadPart / 10;
        }

        uint32 CountThreadsOf(DWORD processId)
        {
            HANDLE const threads = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (threads == INVALID_HANDLE_VALUE)
                return 0;
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            uint32 count = 0;
            if (Thread32First(threads, &entry))
            {
                do
                {
                    if (entry.th32OwnerProcessID == processId)
                        ++count;
                } while (Thread32Next(threads, &entry));
            }
            CloseHandle(threads);
            return count;
        }
    }

    std::optional<ProcessSnapshot> ProcessInfo::SnapshotOf(uint32 processId)
    {
        DWORD const id = static_cast<DWORD>(processId);
        HANDLE const process = id == GetCurrentProcessId()
            ? GetCurrentProcess()
            : OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id);
        if (process == nullptr)
            return std::nullopt;

        ProcessSnapshot snapshot;
        bool anything = false;

        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        if (GetProcessMemoryInfo(process, &counters, counters.cb))
        {
            snapshot.ResidentBytes = static_cast<uint64>(counters.WorkingSetSize);
            anything = true;
        }

        FILETIME creation{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (GetProcessTimes(process, &creation, &exited, &kernel, &user))
        {
            snapshot.CpuMicroseconds = Microseconds(kernel) + Microseconds(user);
            anything = true;
        }

        DWORD handles = 0;
        if (GetProcessHandleCount(process, &handles))
        {
            snapshot.OpenHandles = static_cast<uint32>(handles);
            anything = true;
        }

        IO_COUNTERS io{};
        if (GetProcessIoCounters(process, &io))
        {
            snapshot.DiskReadBytes = static_cast<uint64>(io.ReadTransferCount);
            snapshot.DiskWriteBytes = static_cast<uint64>(io.WriteTransferCount);
            anything = true;
        }

        if (process != GetCurrentProcess())
            CloseHandle(process);

        snapshot.ThreadCount = CountThreadsOf(id);
        if (snapshot.ThreadCount != 0)
            anything = true;
        return anything ? std::optional<ProcessSnapshot>(snapshot) : std::nullopt;
    }
#else
    uint32 ProcessInfo::CurrentProcessId()
    {
        return static_cast<uint32>(getpid());
    }

    uint32 ProcessInfo::CoreCount()
    {
        unsigned const cores = std::thread::hardware_concurrency();
        return cores == 0 ? 1 : static_cast<uint32>(cores);
    }

    namespace
    {
        std::string ProcPath(uint32 processId, char const* leaf)
        {
            return "/proc/" + std::to_string(processId) + "/" + leaf;
        }

        bool ReadStatm(uint32 processId, ProcessSnapshot& snapshot)
        {
            std::ifstream statm(ProcPath(processId, "statm"));
            uint64 pages = 0;
            uint64 resident = 0;
            if (!(statm >> pages >> resident))
                return false;
            long const pageSize = sysconf(_SC_PAGESIZE);
            snapshot.ResidentBytes = resident * static_cast<uint64>(pageSize > 0 ? pageSize : 4096);
            return true;
        }

        bool ReadStat(uint32 processId, ProcessSnapshot& snapshot)
        {
            std::ifstream stat(ProcPath(processId, "stat"));
            std::string text;
            if (!std::getline(stat, text))
                return false;
            std::size_t const close = text.rfind(')');
            if (close == std::string::npos || close + 2 >= text.size())
                return false;

            std::istringstream rest(text.substr(close + 2));
            std::string field;
            uint64 utime = 0;
            uint64 stime = 0;
            uint64 threads = 0;
            for (int index = 3; index <= 20 && rest >> field; ++index)
            {
                if (index == 14)
                    utime = std::strtoull(field.c_str(), nullptr, 10);
                else if (index == 15)
                    stime = std::strtoull(field.c_str(), nullptr, 10);
                else if (index == 20)
                    threads = std::strtoull(field.c_str(), nullptr, 10);
            }
            long const ticks = sysconf(_SC_CLK_TCK);
            uint64 const perSecond = static_cast<uint64>(ticks > 0 ? ticks : 100);
            snapshot.CpuMicroseconds = (utime + stime) * 1000000 / perSecond;
            snapshot.ThreadCount = static_cast<uint32>(threads);
            return true;
        }

        void ReadHandles(uint32 processId, ProcessSnapshot& snapshot)
        {
            std::error_code code;
            std::filesystem::directory_iterator entries(ProcPath(processId, "fd"), code);
            if (code)
                return;
            uint32 count = 0;
            for (auto const& entry : entries)
            {
                (void)entry;
                ++count;
            }
            snapshot.OpenHandles = count;
        }

        void ReadIo(uint32 processId, ProcessSnapshot& snapshot)
        {
            std::ifstream io(ProcPath(processId, "io"));
            std::string line;
            while (std::getline(io, line))
            {
                if (line.rfind("read_bytes:", 0) == 0)
                    snapshot.DiskReadBytes = std::strtoull(line.c_str() + 11, nullptr, 10);
                else if (line.rfind("write_bytes:", 0) == 0)
                    snapshot.DiskWriteBytes = std::strtoull(line.c_str() + 12, nullptr, 10);
            }
        }
    }

    std::optional<ProcessSnapshot> ProcessInfo::SnapshotOf(uint32 processId)
    {
        ProcessSnapshot snapshot;
        bool const memory = ReadStatm(processId, snapshot);
        bool const stat = ReadStat(processId, snapshot);
        if (!memory && !stat)
            return std::nullopt;
        ReadHandles(processId, snapshot);
        ReadIo(processId, snapshot);
        return snapshot;
    }
#endif
}
