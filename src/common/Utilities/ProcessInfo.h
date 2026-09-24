/*
 * Project Ambrose by Imjustchico
 * A snapshot of this process as the operating system sees it, resident memory and thread count, read from the Windows process APIs or from /proc.
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
    };

    class ProcessInfo
    {
    public:
        ProcessInfo() = delete;

        static std::optional<ProcessSnapshot> Snapshot();
    };
}

#endif
