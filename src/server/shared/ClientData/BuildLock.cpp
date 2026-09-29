/*
 * Project Ambrose by Imjustchico
 * Locks the lock file exclusively and without waiting, on Windows through LockFileEx on one byte far past its end and elsewhere through flock, after opening it with every share mode so a holder can remove the name while others hold it open; a lock taken on a file its path no longer names is dropped and tried again a bounded number of times before the lock counts as held, a file system that cannot lock says so apart from other failures, and on release the name is removed only while it still names the locked file, before the lock is dropped and the file closed.
 */

#include "BuildLock.h"
#include "Utf.h"

#include <fmt/format.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string_view>
#include <system_error>

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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
    constexpr int MaxRemovedLockRetries = 16;
    constexpr std::size_t MaxHostNameBytes = 255;
#ifdef _WIN32
    constexpr DWORD LockedByteOffset = 0x40000000;
#endif

    uint64 ProcessId()
    {
#ifdef _WIN32
        return static_cast<uint64>(::GetCurrentProcessId());
#else
        return static_cast<uint64>(::getpid());
#endif
    }

    std::string HostName()
    {
#ifdef _WIN32
        std::array<wchar_t, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
        DWORD size = static_cast<DWORD>(buffer.size());
        if (!::GetComputerNameW(buffer.data(), &size) || size >= buffer.size())
            return {};
        std::optional<std::string> const name = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(buffer.data()), size), Utf::InvalidPolicy::ReplaceWithU_FFFD);
        return name ? *name : std::string();
#else
        std::array<char, MaxHostNameBytes + 1> buffer{};
        if (::gethostname(buffer.data(), MaxHostNameBytes) != 0)
            return {};
        return std::string(buffer.data());
#endif
    }

    std::string HolderText()
    {
        return fmt::format("{} {} {}\n", ProcessId(), std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), HostName());
    }

#ifdef _WIN32
    HANDLE AsHandle(std::intptr_t value)
    {
        return reinterpret_cast<HANDLE>(value);
    }

    bool SameFile(HANDLE first, HANDLE second)
    {
        FILE_ID_INFO firstId{};
        FILE_ID_INFO secondId{};
        if (::GetFileInformationByHandleEx(first, FileIdInfo, &firstId, sizeof(firstId)) && ::GetFileInformationByHandleEx(second, FileIdInfo, &secondId, sizeof(secondId)))
            return firstId.VolumeSerialNumber == secondId.VolumeSerialNumber && std::memcmp(firstId.FileId.Identifier, secondId.FileId.Identifier, sizeof(firstId.FileId.Identifier)) == 0;
        BY_HANDLE_FILE_INFORMATION firstInformation{};
        BY_HANDLE_FILE_INFORMATION secondInformation{};
        return ::GetFileInformationByHandle(first, &firstInformation) && ::GetFileInformationByHandle(second, &secondInformation)
            && firstInformation.dwVolumeSerialNumber == secondInformation.dwVolumeSerialNumber
            && firstInformation.nFileIndexHigh == secondInformation.nFileIndexHigh && firstInformation.nFileIndexLow == secondInformation.nFileIndexLow;
    }

    bool NamesFile(std::filesystem::path const& path, HANDLE handle)
    {
        HANDLE const named = ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (named == INVALID_HANDLE_VALUE)
            return false;
        bool const same = SameFile(handle, named);
        ::CloseHandle(named);
        return same;
    }
#else
    bool NamesFile(std::filesystem::path const& path, int descriptor)
    {
        struct stat opened{};
        struct stat named{};
        return ::fstat(descriptor, &opened) == 0 && ::stat(path.c_str(), &named) == 0 && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino;
    }
#endif
}

BuildLock::~BuildLock()
{
    Release();
}

BuildLockOutcome BuildLock::Take(std::filesystem::path const& path, std::string& error)
{
    Release();
    for (int attempt = 1;; ++attempt)
    {
        if (std::optional<BuildLockOutcome> const outcome = TryTake(path, error))
            return *outcome;
        if (attempt >= MaxRemovedLockRetries)
            return BuildLockOutcome::Held;
    }
}

void BuildLock::Release()
{
#ifdef _WIN32
    if (AsHandle(_handle) == INVALID_HANDLE_VALUE)
        return;
    HANDLE const handle = AsHandle(_handle);
    if (NamesFile(_path, handle))
    {
        std::error_code ignored;
        std::filesystem::remove(_path, ignored);
    }
    OVERLAPPED region{};
    region.Offset = LockedByteOffset;
    ::UnlockFileEx(handle, 0, 1, 0, &region);
    ::CloseHandle(handle);
    _handle = reinterpret_cast<std::intptr_t>(INVALID_HANDLE_VALUE);
#else
    if (_descriptor < 0)
        return;
    if (NamesFile(_path, _descriptor))
        ::unlink(_path.c_str());
    ::flock(_descriptor, LOCK_UN);
    ::close(_descriptor);
    _descriptor = -1;
#endif
}

#ifdef _WIN32
std::optional<BuildLockOutcome> BuildLock::TryTake(std::filesystem::path const& path, std::string& error)
{
    HANDLE const handle = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        error = std::system_category().message(static_cast<int>(::GetLastError()));
        return BuildLockOutcome::Failed;
    }
    OVERLAPPED region{};
    region.Offset = LockedByteOffset;
    if (!::LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &region))
    {
        DWORD const code = ::GetLastError();
        ::CloseHandle(handle);
        if (code == ERROR_LOCK_VIOLATION)
            return BuildLockOutcome::Held;
        error = std::system_category().message(static_cast<int>(code));
        return code == ERROR_NOT_SUPPORTED || code == ERROR_INVALID_FUNCTION ? BuildLockOutcome::Unsupported : BuildLockOutcome::Failed;
    }
    if (!NamesFile(path, handle))
    {
        ::UnlockFileEx(handle, 0, 1, 0, &region);
        ::CloseHandle(handle);
        return std::nullopt;
    }
    _handle = reinterpret_cast<std::intptr_t>(handle);
    _path = path;
    std::string const holder = HolderText();
    LARGE_INTEGER const start{};
    DWORD written = 0;
    if (::SetFilePointerEx(handle, start, nullptr, FILE_BEGIN) && ::WriteFile(handle, holder.data(), static_cast<DWORD>(holder.size()), &written, nullptr))
        ::SetEndOfFile(handle);
    return BuildLockOutcome::Taken;
}
#else
std::optional<BuildLockOutcome> BuildLock::TryTake(std::filesystem::path const& path, std::string& error)
{
    int descriptor = -1;
    do
        descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0)
    {
        error = std::generic_category().message(errno);
        return BuildLockOutcome::Failed;
    }
    int locked = 0;
    do
        locked = ::flock(descriptor, LOCK_EX | LOCK_NB);
    while (locked != 0 && errno == EINTR);
    if (locked != 0)
    {
        int const code = errno;
        ::close(descriptor);
        if (code == EWOULDBLOCK)
            return BuildLockOutcome::Held;
        error = std::generic_category().message(code);
        return code == ENOLCK || code == ENOTSUP || code == EOPNOTSUPP || code == ENOSYS ? BuildLockOutcome::Unsupported : BuildLockOutcome::Failed;
    }
    if (!NamesFile(path, descriptor))
    {
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
        return std::nullopt;
    }
    _descriptor = descriptor;
    _path = path;
    std::string const holder = HolderText();
    if (::ftruncate(_descriptor, 0) == 0)
    {
        std::string_view pending = holder;
        off_t offset = 0;
        while (!pending.empty())
        {
            ssize_t const written = ::pwrite(_descriptor, pending.data(), pending.size(), offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
                break;
            pending.remove_prefix(static_cast<std::size_t>(written));
            offset += static_cast<off_t>(written);
        }
    }
    return BuildLockOutcome::Taken;
}
#endif
