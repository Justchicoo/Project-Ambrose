/*
 * Project Ambrose by Imjustchico
 * A snapshot of a process as the operating system sees it, resident memory, thread and handle counts, the processor time it has used and the bytes it has read and written, read from the Windows process APIs or from /proc. Processor time is given as the total the process has ever used rather than as a share, because a share is only meaningful between two readings and the caller is the one that knows how far apart its readings are; the number of cores is given beside it so that share can be worked out. Any process may be asked about, not only this one, because the supervisor watches the apps it runs and they are not itself.
 */

#ifndef AMBROSE_PROCESSINFO_H
#define AMBROSE_PROCESSINFO_H

#include "Types.h"

#include <optional>

namespace Ambrose
{
    struct ProcessSnapshot
    {
        uint64 ResidentBytes = 0;
        uint32 ThreadCount = 0;
        uint32 OpenHandles = 0;
        uint64 CpuMicroseconds = 0;
        uint64 DiskReadBytes = 0;
        uint64 DiskWriteBytes = 0;
    };

    class ProcessInfo
    {
    public:
        ProcessInfo() = delete;

        static std::optional<ProcessSnapshot> Snapshot();
        static std::optional<ProcessSnapshot> SnapshotOf(uint32 processId);
        static uint32 CurrentProcessId();
        static uint32 CoreCount();

        static double CpuShare(ProcessSnapshot const& earlier, ProcessSnapshot const& later, uint64 elapsedMicroseconds, uint32 cores);
    };
}

#endif
