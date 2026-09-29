/*
 * Project Ambrose by Imjustchico
 * An operating system lock on a lock file beside something a process builds from the user's install, such as a type dump or a class file, which keeps processes started together from building the same thing twice: the system releases it however its holder ends, the holder writes its process id, time and host name into the file and removes the file's name when it is done only while that name still names the file it locked, and a name that was removed or replaced while it was being locked is locked again.
 */

#ifndef AMBROSE_BUILDLOCK_H
#define AMBROSE_BUILDLOCK_H

#include "Types.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

enum class BuildLockOutcome : uint8
{
    Taken,
    Held,
    Unsupported,
    Failed
};

class BuildLock
{
public:
    BuildLock() = default;
    ~BuildLock();

    BuildLock(BuildLock const&) = delete;
    BuildLock& operator=(BuildLock const&) = delete;

    BuildLockOutcome Take(std::filesystem::path const& path, std::string& error);
    void Release();

private:
    std::optional<BuildLockOutcome> TryTake(std::filesystem::path const& path, std::string& error);

#ifdef _WIN32
    std::intptr_t _handle = -1;
#else
    int _descriptor = -1;
#endif
    std::filesystem::path _path;
};

#endif
