/*
 * Project Ambrose by Imjustchico
 * Reads the process's working set and thread count from GetProcessMemoryInfo and a Toolhelp thread snapshot on Windows, and from /proc/self/statm and /proc/self/status elsewhere.
 */

#include "ProcessInfo.h"

#ifdef _WIN32
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif

namespace Ambrose
{
#ifdef _WIN32
    std::optional<ProcessSnapshot> ProcessInfo::Snapshot()
    {
        ProcessSnapshot snapshot;
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb))
            return std::nullopt;
        snapshot.ResidentBytes = static_cast<uint64>(counters.WorkingSetSize);

        HANDLE const threads = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (threads == INVALID_HANDLE_VALUE)
            return std::nullopt;
        DWORD const self = GetCurrentProcessId();
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        uint32 count = 0;
        if (Thread32First(threads, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID == self)
                    ++count;
            } while (Thread32Next(threads, &entry));
        }
        CloseHandle(threads);
        snapshot.ThreadCount = count;
        return snapshot;
    }
#else
    std::optional<ProcessSnapshot> ProcessInfo::Snapshot()
    {
        ProcessSnapshot snapshot;
        std::ifstream statm("/proc/self/statm");
        uint64 pages = 0;
        uint64 resident = 0;
        if (!(statm >> pages >> resident))
            return std::nullopt;
        long const pageSize = sysconf(_SC_PAGESIZE);
        snapshot.ResidentBytes = resident * static_cast<uint64>(pageSize > 0 ? pageSize : 4096);

        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line))
        {
            if (line.rfind("Threads:", 0) == 0)
            {
                snapshot.ThreadCount = static_cast<uint32>(std::stoul(line.substr(8)));
                break;
            }
        }
        return snapshot;
    }
#endif
}
