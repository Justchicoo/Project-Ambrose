/*
 * Project Ambrose by Imjustchico
 * The jail on each system. On Linux the root is an O_PATH descriptor; a request's folders resolve in one openat2 call beneath the root with no link followed when the kernel has it, the probe treating ENOSYS and a seccomp EPERM alike as missing, and otherwise, or to name the link or file that stopped it, component by component with O_NOFOLLOW, each step checked with fstat on the O_PATH descriptor so a FIFO, a device or a socket is refused before it is ever opened; a link that stays inside is expanded against the folders already walked only when the root's policy follows one. On Windows every component opens relative to its parent with NtCreateFile and FILE_OPEN_REPARSE_POINT for attributes alone, a junction, symbolic link, mount point or AF_UNIX socket, a device or anything not on a disk is refused, dedup, compressed and cloud files open as they are, and the final path of what was reached is compared with the root's, which also turns a short 8.3 name into its long one. A refused link's target is read for the audit row, a listing reads the folder's own handle, a read reopens the entry and compares its identity, a create takes its space reservation before the file exists and removes the file if writing it fails, and a rename or a removal acts on the entry reopened relative to its parent after its identity is compared.
 */

#include "FileJail.h"
#include "SpaceGuard.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <winternl.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <memory>
#if defined(__linux__)
#include <sys/syscall.h>
#if __has_include(<linux/openat2.h>)
#include <linux/openat2.h>
#endif
#endif
#if defined(SYS_openat2) && defined(RESOLVE_BENEATH) && defined(RESOLVE_NO_SYMLINKS) && defined(RESOLVE_NO_MAGICLINKS)
#define AMBROSE_JAIL_OPENAT2 1
#endif
#endif

namespace
{
    constexpr std::string_view LinkMessage = "A symbolic link, junction or mount point is on this path, and the panel follows none out of a root";
    constexpr std::string_view OutsideMessage = "This path leads outside its root";
    constexpr std::string_view ChangedMessage = "The file changed while it was being opened, so it was not read";
    constexpr std::string_view NotFolderMessage = "A part of this path is not a folder";

    std::string Utf8Of(std::filesystem::path const& path)
    {
        try
        {
            std::u8string const text = path.u8string();
            return std::string(text.begin(), text.end());
        }
        catch (std::exception const&)
        {
            return {};
        }
    }

    bool RefuseSpecial(Ambrose::EntryKind kind, Ambrose::JailError& error, std::string const& resolved)
    {
        switch (kind)
        {
            case Ambrose::EntryKind::Fifo:
                error.Set(Ambrose::JailFailure::Refused, "fifo", "This is a named pipe, which the panel never opens", resolved);
                return true;
            case Ambrose::EntryKind::Device:
                error.Set(Ambrose::JailFailure::Refused, "device", "This is a device, which the panel never opens", resolved);
                return true;
            case Ambrose::EntryKind::Socket:
                error.Set(Ambrose::JailFailure::Refused, "socket", "This is a socket, which the panel never opens", resolved);
                return true;
            case Ambrose::EntryKind::Other:
                error.Set(Ambrose::JailFailure::Refused, "special", "This is neither a file nor a folder, so the panel never opens it", resolved);
                return true;
            default:
                return false;
        }
    }

    void MarkOpenable(Ambrose::JailListed& listed, std::string_view rawName)
    {
        if (!Utf::IsValidUtf8(rawName))
        {
            listed.Openable = false;
            listed.Problem = "This name is not valid UTF-8, so the panel cannot name it; rename it on the machine";
            return;
        }
        std::string reason;
        if (Ambrose::JailPaths::CheckName(rawName, reason) != Ambrose::JailRefusal::None)
        {
            listed.Openable = false;
            listed.Problem = reason + "; rename it on the machine to open it here";
            return;
        }
        switch (listed.Kind)
        {
            case Ambrose::EntryKind::Link:
                listed.Openable = false;
                listed.Problem = "A link, junction or mount point, which the panel follows none of";
                break;
            case Ambrose::EntryKind::Fifo:
                listed.Openable = false;
                listed.Problem = "A named pipe, which the panel never opens";
                break;
            case Ambrose::EntryKind::Device:
                listed.Openable = false;
                listed.Problem = "A device, which the panel never opens";
                break;
            case Ambrose::EntryKind::Socket:
                listed.Openable = false;
                listed.Problem = "A socket, which the panel never opens";
                break;
            case Ambrose::EntryKind::Other:
                listed.Openable = false;
                listed.Problem = "Neither a file nor a folder, so the panel never opens it";
                break;
            default:
                break;
        }
    }
}

Ambrose::JailHandle::JailHandle(Native native) noexcept : _native(native)
{
}

Ambrose::JailHandle::~JailHandle()
{
    Reset();
}

Ambrose::JailHandle::JailHandle(JailHandle&& other) noexcept : _native(std::exchange(other._native, Invalid()))
{
}

Ambrose::JailHandle& Ambrose::JailHandle::operator=(JailHandle&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        _native = std::exchange(other._native, Invalid());
    }
    return *this;
}

void Ambrose::JailError::Set(JailFailure failure, std::string code, std::string message, std::string resolved)
{
    Failure = failure;
    Code = std::move(code);
    Message = std::move(message);
    Resolved = std::move(resolved);
}

std::string Ambrose::JailEntry::Relative() const
{
    std::string text;
    for (std::string const& component : Components)
    {
        if (!text.empty())
            text.push_back('/');
        text += component;
    }
    return text;
}

std::string_view Ambrose::FileJail::KindName(EntryKind kind) noexcept
{
    switch (kind)
    {
        case EntryKind::File: return "file";
        case EntryKind::Folder: return "folder";
        case EntryKind::Link: return "link";
        case EntryKind::Fifo: return "fifo";
        case EntryKind::Device: return "device";
        case EntryKind::Socket: return "socket";
        case EntryKind::Other: break;
    }
    return "other";
}

std::string Ambrose::FileJail::HostText(std::filesystem::path const& path)
{
    return Utf8Of(path);
}

#ifdef _WIN32

namespace
{
    constexpr ULONG ShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    constexpr DWORD TagMountPoint = 0xA0000003u;
    constexpr DWORD TagSymbolicLink = 0xA000000Cu;
    constexpr DWORD TagUnixSocket = 0x80000023u;
    constexpr ACCESS_MASK FolderAccess = FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE;
    constexpr DWORD FolderFlags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;

    struct RenameInformation
    {
        BOOLEAN ReplaceIfExists;
        HANDLE RootDirectory;
        ULONG FileNameLength;
        WCHAR FileName[1];
    };

    bool NameSurrogate(DWORD tag) noexcept
    {
        return (tag & 0x20000000u) != 0;
    }

    std::wstring Wide(std::string_view utf8)
    {
        if (utf8.empty())
            return {};
        int const length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        if (length <= 0)
            return {};
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
        return wide;
    }

    std::optional<std::string> Narrow(std::wstring_view wide, bool strict)
    {
        if (wide.empty())
            return std::string();
        DWORD const flags = strict ? WC_ERR_INVALID_CHARS : 0;
        int const length = WideCharToMultiByte(CP_UTF8, flags, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0)
            return std::nullopt;
        std::string narrow(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, flags, wide.data(), static_cast<int>(wide.size()), narrow.data(), length, nullptr, nullptr);
        return narrow;
    }

    int64 EpochMs(uint64 ticks) noexcept
    {
        return static_cast<int64>(ticks / 10000) - 11644473600000LL;
    }

    int64 EpochMs(FILETIME const& time) noexcept
    {
        return EpochMs((static_cast<uint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime);
    }

    Ambrose::EntryKind KindFrom(DWORD attributes, DWORD tag, bool disk) noexcept
    {
        if (!disk || (attributes & FILE_ATTRIBUTE_DEVICE) != 0)
            return Ambrose::EntryKind::Device;
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            if (tag == TagUnixSocket)
                return Ambrose::EntryKind::Socket;
            if (NameSurrogate(tag))
                return Ambrose::EntryKind::Link;
        }
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? Ambrose::EntryKind::Folder : Ambrose::EntryKind::File;
    }

    Ambrose::FileIdentity IdentityFrom(HANDLE handle, BY_HANDLE_FILE_INFORMATION const& info)
    {
        Ambrose::FileIdentity identity;
        identity.Known = true;
        FILE_ID_INFO id = {};
        if (GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id)))
        {
            identity.Volume = id.VolumeSerialNumber;
            std::memcpy(&identity.Low, id.FileId.Identifier, sizeof(identity.Low));
            std::memcpy(&identity.High, id.FileId.Identifier + sizeof(identity.Low), sizeof(identity.High));
            return identity;
        }
        identity.Volume = info.dwVolumeSerialNumber;
        identity.Low = (static_cast<uint64>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
        return identity;
    }

    bool Inspect(HANDLE handle, Ambrose::JailStat& seen, DWORD& code)
    {
        BY_HANDLE_FILE_INFORMATION info = {};
        if (!GetFileInformationByHandle(handle, &info))
        {
            code = GetLastError();
            return false;
        }
        DWORD tag = 0;
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            FILE_ATTRIBUTE_TAG_INFO tagged = {};
            if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tagged, sizeof(tagged)))
                tag = tagged.ReparseTag;
        }
        bool const disk = GetFileType(handle) == FILE_TYPE_DISK;
        seen.Kind = KindFrom(info.dwFileAttributes, tag, disk);
        seen.Size = (static_cast<uint64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        seen.ModifiedEpochMs = EpochMs(info.ftLastWriteTime);
        seen.Links = info.nNumberOfLinks;
        seen.Identity = IdentityFrom(handle, info);
        return true;
    }

    std::optional<std::wstring> FinalPath(HANDLE handle)
    {
        std::wstring buffer(512, L'\0');
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            DWORD const length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (length == 0)
                return std::nullopt;
            if (length < buffer.size())
            {
                buffer.resize(length);
                return buffer;
            }
            buffer.assign(static_cast<std::size_t>(length) + 1, L'\0');
        }
        return std::nullopt;
    }

    std::wstring Displayed(std::wstring const& final)
    {
        constexpr std::wstring_view Unc = L"\\\\?\\UNC\\";
        constexpr std::wstring_view Local = L"\\\\?\\";
        if (final.starts_with(Unc))
            return L"\\\\" + final.substr(Unc.size());
        if (final.starts_with(Local))
            return final.substr(Local.size());
        return final;
    }

    std::string DisplayedText(std::wstring const& final)
    {
        return Narrow(Displayed(final), false).value_or(std::string());
    }

    std::optional<std::vector<std::string>> Beneath(std::wstring const& root, std::wstring const& final)
    {
        if (root.empty() || final.size() < root.size())
            return std::nullopt;
        if (CompareStringOrdinal(final.data(), static_cast<int>(root.size()), root.data(), static_cast<int>(root.size()), TRUE) != CSTR_EQUAL)
            return std::nullopt;
        std::vector<std::string> components;
        std::wstring_view rest(final);
        rest.remove_prefix(root.size());
        if (rest.empty())
            return components;
        if (root.back() != L'\\')
        {
            if (rest.front() != L'\\')
                return std::nullopt;
            rest.remove_prefix(1);
        }
        while (!rest.empty())
        {
            std::size_t const slash = rest.find(L'\\');
            std::wstring_view const part = rest.substr(0, slash);
            if (!part.empty())
            {
                std::optional<std::string> name = Narrow(part, true);
                if (!name)
                    return std::nullopt;
                components.push_back(std::move(*name));
            }
            if (slash == std::wstring_view::npos)
                break;
            rest.remove_prefix(slash + 1);
        }
        return components;
    }

    void FromWin32(Ambrose::JailError& error, DWORD code, std::string resolved);

    void FromWin32(Ambrose::JailError& error, DWORD code, std::filesystem::path const& resolved)
    {
        FromWin32(error, code, Ambrose::FileJail::HostText(resolved));
    }

    void FromWin32(Ambrose::JailError& error, DWORD code, std::string resolved)
    {
        switch (code)
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
            case ERROR_BAD_NETPATH:
                error.Set(Ambrose::JailFailure::Missing, "not_found", "Nothing is at this path", std::move(resolved));
                return;
            case ERROR_DIRECTORY:
                error.Set(Ambrose::JailFailure::Missing, "not_found", std::string(NotFolderMessage), std::move(resolved));
                return;
            case ERROR_SHARING_VIOLATION:
            case ERROR_LOCK_VIOLATION:
                error.Set(Ambrose::JailFailure::Conflict, "busy", "Another program holds this file open in a way that keeps it from being opened here; try again once it lets go",
                    std::move(resolved));
                return;
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                error.Set(Ambrose::JailFailure::Conflict, "exists", "Something is already at this path", std::move(resolved));
                return;
            case ERROR_DIR_NOT_EMPTY:
                error.Set(Ambrose::JailFailure::Conflict, "not_empty", "This folder is not empty", std::move(resolved));
                return;
            case ERROR_ACCESS_DENIED:
                error.Set(Ambrose::JailFailure::Failed, "denied", "The service account may not open this path", std::move(resolved));
                return;
            default:
                error.Set(Ambrose::JailFailure::Failed, "io", fmt::format("The path could not be opened (Windows error {})", code), std::move(resolved));
                return;
        }
    }

    HANDLE OpenRelative(HANDLE parent, std::wstring const& name, ACCESS_MASK access, ULONG options, DWORD& code)
    {
        UNICODE_STRING text;
        text.Buffer = const_cast<PWSTR>(name.c_str());
        text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
        text.MaximumLength = text.Length;
        OBJECT_ATTRIBUTES attributes;
        InitializeObjectAttributes(&attributes, &text, OBJ_CASE_INSENSITIVE, parent, nullptr);
        IO_STATUS_BLOCK status = {};
        HANDLE handle = nullptr;
        NTSTATUS const result = NtCreateFile(&handle, access, &attributes, &status, nullptr, 0, ShareAll, FILE_OPEN,
            options | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT, nullptr, 0);
        if (result < 0)
        {
            code = RtlNtStatusToDosError(result);
            return nullptr;
        }
        return handle;
    }

    HANDLE OpenFolderForRename(Ambrose::JailEntry const& folder, bool subdirectory, bool deleteChild, bool addFile, DWORD& code)
    {
        ACCESS_MASK const access = FILE_LIST_DIRECTORY | FILE_TRAVERSE | (addFile ? FILE_ADD_FILE : 0) | (subdirectory ? FILE_ADD_SUBDIRECTORY : 0)
            | (deleteChild ? FILE_DELETE_CHILD : 0) | FILE_READ_ATTRIBUTES | SYNCHRONIZE;
        HANDLE const handle = CreateFileW(folder.HostPath.wstring().c_str(), access, ShareAll, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            code = GetLastError();
            return nullptr;
        }
        Ambrose::JailStat current;
        if (!Inspect(handle, current, code) || current.Kind != Ambrose::EntryKind::Folder || !current.Identity.Same(folder.Stat.Identity))
        {
            CloseHandle(handle);
            code = ERROR_ACCESS_DENIED;
            return nullptr;
        }
        return handle;
    }

    struct ReparseTarget
    {
        std::wstring Path;
        bool Relative = false;
    };

    std::optional<ReparseTarget> ReadTarget(HANDLE handle)
    {
        std::vector<unsigned char> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
        DWORD returned = 0;
        if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) || returned < 16)
            return std::nullopt;
        DWORD tag = 0;
        std::memcpy(&tag, buffer.data(), sizeof(tag));
        auto const word = [&buffer](std::size_t at)
        {
            USHORT value = 0;
            std::memcpy(&value, buffer.data() + at, sizeof(value));
            return value;
        };
        std::size_t start = 0;
        ReparseTarget target;
        if (tag == TagSymbolicLink)
        {
            if (returned < 20)
                return std::nullopt;
            ULONG flags = 0;
            std::memcpy(&flags, buffer.data() + 16, sizeof(flags));
            target.Relative = (flags & 1u) != 0;
            start = 20;
        }
        else if (tag == TagMountPoint)
            start = 16;
        else
            return std::nullopt;
        auto const text = [&](USHORT offset, USHORT length)
        {
            std::size_t const begin = start + offset;
            if (begin + length > returned)
                return std::wstring();
            std::wstring value(length / sizeof(wchar_t), L'\0');
            std::memcpy(value.data(), buffer.data() + begin, value.size() * sizeof(wchar_t));
            return value;
        };
        target.Path = text(word(12), word(14));
        if (target.Path.empty())
        {
            target.Path = text(word(8), word(10));
            if (target.Path.starts_with(L"\\??\\"))
                target.Path.erase(0, 4);
        }
        if (target.Path.empty())
            return std::nullopt;
        return target;
    }

    std::string TargetText(std::optional<ReparseTarget> const& target, std::filesystem::path const& parentHost, std::filesystem::path const& fallback)
    {
        if (!target)
            return Ambrose::FileJail::HostText(fallback);
        std::filesystem::path const path(target->Path);
        return Ambrose::FileJail::HostText((target->Relative ? parentHost / path : path).lexically_normal());
    }

    HANDLE ReopenRoot(Ambrose::JailRoot const& root, DWORD& code)
    {
        HANDLE const folder = CreateFileW(root.GetPath().wstring().c_str(), FolderAccess, ShareAll, nullptr, OPEN_EXISTING, FolderFlags | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (folder == INVALID_HANDLE_VALUE)
        {
            code = GetLastError();
            return nullptr;
        }
        Ambrose::JailStat again;
        DWORD ignored = 0;
        if (!Inspect(folder, again, ignored) || !again.Identity.Same(root.GetIdentity()))
        {
            CloseHandle(folder);
            code = ERROR_FILE_INVALID;
            return nullptr;
        }
        return folder;
    }

    HANDLE OpenFolderAgain(HANDLE parent, std::wstring const& name, bool followLink, Ambrose::FileIdentity const& expected, DWORD& code)
    {
        HANDLE const folder = OpenRelative(parent, name, FolderAccess, FILE_DIRECTORY_FILE | (followLink ? 0 : FILE_OPEN_REPARSE_POINT), code);
        if (!folder)
            return nullptr;
        Ambrose::JailStat again;
        DWORD ignored = 0;
        if (!Inspect(folder, again, ignored) || !again.Identity.Same(expected))
        {
            CloseHandle(folder);
            code = ERROR_FILE_INVALID;
            return nullptr;
        }
        return folder;
    }
}

Ambrose::JailHandle::Native Ambrose::JailHandle::Invalid() noexcept
{
    return nullptr;
}

bool Ambrose::JailHandle::Valid() const noexcept
{
    return _native != nullptr && _native != INVALID_HANDLE_VALUE;
}

void Ambrose::JailHandle::Reset() noexcept
{
    if (Valid())
        CloseHandle(_native);
    _native = Invalid();
}

Ambrose::JailHandle Ambrose::JailHandle::Duplicate() const
{
    if (!Valid())
        return JailHandle();
    HANDLE copy = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), _native, GetCurrentProcess(), &copy, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return JailHandle();
    return JailHandle(copy);
}

std::shared_ptr<Ambrose::JailRoot> Ambrose::JailRoot::Open(std::filesystem::path const& path, std::string& error)
{
    std::shared_ptr<JailRoot> root(new JailRoot());
    HANDLE const raw = CreateFileW(path.wstring().c_str(), FolderAccess, ShareAll, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (raw == INVALID_HANDLE_VALUE)
    {
        DWORD const code = GetLastError();
        error = code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND ? std::string("the folder does not exist") : fmt::format("the folder could not be opened (Windows error {})", code);
        return nullptr;
    }
    root->_handle = JailHandle(raw);
    DWORD code = 0;
    JailStat seen;
    if (!Inspect(raw, seen, code))
    {
        error = fmt::format("the folder could not be read (Windows error {})", code);
        return nullptr;
    }
    if (seen.Kind != EntryKind::Folder)
    {
        error = "it is not a folder";
        return nullptr;
    }
    std::optional<std::wstring> const final = FinalPath(raw);
    if (!final)
    {
        error = fmt::format("the folder's final path could not be read (Windows error {})", GetLastError());
        return nullptr;
    }
    root->_final = *final;
    root->_path = std::filesystem::path(Displayed(*final));
    root->_identity = seen.Identity;
    return root;
}

std::optional<Ambrose::JailEntry> Ambrose::FileJail::Resolve(JailRoot const& root, JailPath const& path, LinkPolicy links, JailError& error)
{
    JailEntry entry;
    DWORD code = 0;
    if (path.IsRoot())
    {
        HANDLE const folder = ReopenRoot(root, code);
        if (!folder)
        {
            FromWin32(error, code, HostText(root.GetPath()));
            return std::nullopt;
        }
        entry.Handle = JailHandle(folder);
        if (!Inspect(entry.Handle.Get(), entry.Stat, code))
        {
            FromWin32(error, code, HostText(root.GetPath()));
            return std::nullopt;
        }
        entry.HostPath = root.GetPath();
        return entry;
    }
    JailHandle current = root.GetHandle().Duplicate();
    if (!current.Valid())
    {
        error.Set(JailFailure::Failed, "io", "The root could not be opened again");
        return std::nullopt;
    }
    std::deque<std::string> queue(path.Components.begin(), path.Components.end());
    std::filesystem::path currentHost = root.GetPath();
    std::size_t followed = 0;
    while (!queue.empty())
    {
        std::string const name = queue.front();
        queue.pop_front();
        std::wstring const wide = Wide(name);
        std::filesystem::path host = currentHost / std::filesystem::path(wide);
        if (wide.empty())
        {
            error.Set(JailFailure::Missing, "not_found", "Nothing is at this path", HostText(host));
            return std::nullopt;
        }
        HANDLE raw = OpenRelative(current.Get(), wide, FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_OPEN_REPARSE_POINT, code);
        if (!raw)
        {
            FromWin32(error, code, HostText(host));
            return std::nullopt;
        }
        JailHandle opened(raw);
        JailStat seen;
        bool followedHere = false;
        if (!Inspect(opened.Get(), seen, code))
        {
            FromWin32(error, code, HostText(host));
            return std::nullopt;
        }
        if (seen.Kind == EntryKind::Link)
        {
            std::optional<ReparseTarget> const target = ReadTarget(opened.Get());
            std::string const resolved = TargetText(target, currentHost, host);
            if (links == LinkPolicy::Refuse)
            {
                error.Set(JailFailure::Refused, "link", std::string(LinkMessage), resolved);
                return std::nullopt;
            }
            if (++followed > MaxLinksFollowed)
            {
                error.Set(JailFailure::Refused, "link", "This path follows too many links", resolved);
                return std::nullopt;
            }
            raw = OpenRelative(current.Get(), wide, FILE_READ_ATTRIBUTES | SYNCHRONIZE, 0, code);
            if (!raw)
            {
                FromWin32(error, code, resolved);
                return std::nullopt;
            }
            opened = JailHandle(raw);
            std::optional<std::wstring> const final = FinalPath(opened.Get());
            std::optional<std::vector<std::string>> const inside = final ? Beneath(root.GetFinalPath(), *final) : std::nullopt;
            if (!inside)
            {
                error.Set(JailFailure::Refused, "outside", std::string(OutsideMessage), final ? DisplayedText(*final) : resolved);
                return std::nullopt;
            }
            if (!Inspect(opened.Get(), seen, code))
            {
                FromWin32(error, code, resolved);
                return std::nullopt;
            }
            host = std::filesystem::path(Displayed(*final));
            followedHere = true;
        }
        if (RefuseSpecial(seen.Kind, error, HostText(host)))
            return std::nullopt;
        if (!queue.empty())
        {
            if (seen.Kind != EntryKind::Folder)
            {
                error.Set(JailFailure::Missing, "not_found", std::string(NotFolderMessage), HostText(host));
                return std::nullopt;
            }
            HANDLE const folder = OpenFolderAgain(current.Get(), wide, followedHere, seen.Identity, code);
            if (!folder)
            {
                FromWin32(error, code, HostText(host));
                return std::nullopt;
            }
            current = JailHandle(folder);
            currentHost = host;
            continue;
        }
        if (seen.Kind == EntryKind::Folder)
        {
            HANDLE const folder = OpenFolderAgain(current.Get(), wide, followedHere, seen.Identity, code);
            if (!folder)
            {
                FromWin32(error, code, HostText(host));
                return std::nullopt;
            }
            entry.Handle = JailHandle(folder);
        }
        else
            entry.Handle = std::move(opened);
        entry.Parent = std::move(current);
        entry.Leaf = name;
        entry.Stat = seen;
    }
    std::optional<std::wstring> const final = FinalPath(entry.Handle.Get());
    if (!final)
    {
        error.Set(JailFailure::Failed, "io", fmt::format("The final name of this path could not be read (Windows error {})", GetLastError()));
        return std::nullopt;
    }
    std::optional<std::vector<std::string>> inside = Beneath(root.GetFinalPath(), *final);
    if (!inside)
    {
        error.Set(JailFailure::Refused, "outside", std::string(OutsideMessage), DisplayedText(*final));
        return std::nullopt;
    }
    entry.Components = std::move(*inside);
    entry.HostPath = std::filesystem::path(Displayed(*final));
    return entry;
}

bool Ambrose::FileJail::List(JailEntry const& folder, std::size_t mostEntries, std::vector<JailListed>& entries, bool& truncated, JailError& error)
{
    truncated = false;
    if (folder.Stat.Kind != EntryKind::Folder || !folder.Handle.Valid())
    {
        error.Set(JailFailure::Failed, "not_a_folder", "Only a folder can be listed");
        return false;
    }
    std::vector<LONGLONG> storage(64 * 1024 / sizeof(LONGLONG));
    DWORD const size = static_cast<DWORD>(storage.size() * sizeof(LONGLONG));
    bool extended = true;
    bool first = true;
    uint64 const volume = folder.Stat.Identity.Volume;
    for (;;)
    {
        FILE_INFO_BY_HANDLE_CLASS const kind = extended ? (first ? FileIdExtdDirectoryRestartInfo : FileIdExtdDirectoryInfo)
                                                        : (first ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo);
        if (!GetFileInformationByHandleEx(folder.Handle.Get(), kind, storage.data(), size))
        {
            DWORD const code = GetLastError();
            if (code == ERROR_NO_MORE_FILES)
                return true;
            if (first && extended && (code == ERROR_INVALID_PARAMETER || code == ERROR_NOT_SUPPORTED || code == ERROR_INVALID_LEVEL || code == ERROR_INVALID_FUNCTION))
            {
                extended = false;
                continue;
            }
            FromWin32(error, code, HostText(folder.HostPath));
            return false;
        }
        first = false;
        unsigned char const* at = reinterpret_cast<unsigned char const*>(storage.data());
        for (;;)
        {
            ULONG next = 0;
            std::wstring name;
            DWORD attributes = 0;
            DWORD tag = 0;
            JailListed listed;
            listed.Identity.Known = true;
            listed.Identity.Volume = volume;
            if (extended)
            {
                auto const* item = reinterpret_cast<FILE_ID_EXTD_DIR_INFO const*>(at);
                next = item->NextEntryOffset;
                name.assign(item->FileName, item->FileNameLength / sizeof(WCHAR));
                attributes = item->FileAttributes;
                tag = item->ReparsePointTag;
                listed.Size = static_cast<uint64>(item->EndOfFile.QuadPart);
                listed.ModifiedEpochMs = EpochMs(static_cast<uint64>(item->LastWriteTime.QuadPart));
                std::memcpy(&listed.Identity.Low, item->FileId.Identifier, sizeof(listed.Identity.Low));
                std::memcpy(&listed.Identity.High, item->FileId.Identifier + sizeof(listed.Identity.Low), sizeof(listed.Identity.High));
            }
            else
            {
                auto const* item = reinterpret_cast<FILE_ID_BOTH_DIR_INFO const*>(at);
                next = item->NextEntryOffset;
                name.assign(item->FileName, item->FileNameLength / sizeof(WCHAR));
                attributes = item->FileAttributes;
                tag = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ? item->EaSize : 0;
                listed.Size = static_cast<uint64>(item->EndOfFile.QuadPart);
                listed.ModifiedEpochMs = EpochMs(static_cast<uint64>(item->LastWriteTime.QuadPart));
                listed.Identity.Low = static_cast<uint64>(item->FileId.QuadPart);
            }
            if (name != L"." && name != L"..")
            {
                if (entries.size() >= mostEntries)
                {
                    truncated = true;
                    return true;
                }
                listed.Kind = KindFrom(attributes, tag, true);
                if (listed.Kind == EntryKind::File && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    listed.Kind = EntryKind::Folder;
                if (listed.Kind == EntryKind::Folder)
                    listed.Size = 0;
                std::optional<std::string> const strict = Narrow(name, true);
                listed.Name = strict ? *strict : Narrow(name, false).value_or(std::string());
                if (!strict)
                {
                    listed.Openable = false;
                    listed.Problem = "This name is not valid Unicode, so the panel cannot name it; rename it on the machine";
                }
                else
                    MarkOpenable(listed, listed.Name);
                entries.push_back(std::move(listed));
            }
            if (next == 0)
                break;
            at += next;
        }
    }
}

bool Ambrose::FileJail::Read(JailEntry const& entry, uint64 offset, std::size_t length, std::string& bytes, JailError& error)
{
    bytes.clear();
    if (entry.Stat.Kind != EntryKind::File || !entry.Handle.Valid())
    {
        error.Set(JailFailure::Failed, "not_a_file", "Only a file can be read");
        return false;
    }
    HANDLE const raw = ReOpenFile(entry.Handle.Get(), GENERIC_READ, ShareAll, FILE_FLAG_SEQUENTIAL_SCAN);
    if (raw == INVALID_HANDLE_VALUE)
    {
        FromWin32(error, GetLastError(), entry.HostPath);
        return false;
    }
    JailHandle handle(raw);
    JailStat now;
    DWORD code = 0;
    if (!Inspect(raw, now, code) || now.Kind != EntryKind::File || !now.Identity.Same(entry.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    LARGE_INTEGER position = {};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(raw, position, nullptr, FILE_BEGIN))
    {
        FromWin32(error, GetLastError(), entry.HostPath);
        return false;
    }
    bytes.resize(length);
    std::size_t done = 0;
    while (done < length)
    {
        DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(length - done, 1u << 20));
        DWORD got = 0;
        if (!ReadFile(raw, bytes.data() + done, chunk, &got, nullptr))
        {
            FromWin32(error, GetLastError(), entry.HostPath);
            bytes.clear();
            return false;
        }
        if (got == 0)
            break;
        done += got;
    }
    bytes.resize(done);
    return true;
}

bool Ambrose::FileJail::Create(JailRoot const& root, JailPath const& path, std::string_view contents, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal)
{
    if (path.IsRoot())
    {
        error.Set(JailFailure::Failed, "invalid", "A new file needs a name");
        return false;
    }
    std::optional<JailEntry> const parent = Resolve(root, path.Parent(), LinkPolicy::Refuse, error);
    if (!parent)
        return false;
    if (parent->Stat.Kind != EntryKind::Folder)
    {
        error.Set(JailFailure::Missing, "not_found", std::string(NotFolderMessage), HostText(parent->HostPath));
        return false;
    }
    DWORD parentCode = 0;
    JailHandle safeParent(OpenFolderForRename(*parent, false, false, true, parentCode));
    if (!safeParent.Valid())
    {
        FromWin32(error, parentCode, parent->HostPath);
        return false;
    }
    std::filesystem::path const host = parent->HostPath / std::filesystem::path(Wide(path.Leaf()));
    SpaceRefusal held;
    std::string why;
    std::optional<SpaceReservation> reservation = guard.Reserve(parent->HostPath, contents.size(), held, why);
    if (!reservation)
    {
        if (refusal)
            *refusal = held;
        error.Set(held.Volume.empty() ? JailFailure::Failed : JailFailure::NoSpace, held.Volume.empty() ? "space_unknown" : "space", why, HostText(host));
        return false;
    }
    std::wstring const wide = Wide(path.Leaf());
    UNICODE_STRING text;
    text.Buffer = const_cast<PWSTR>(wide.c_str());
    text.Length = static_cast<USHORT>(wide.size() * sizeof(wchar_t));
    text.MaximumLength = text.Length;
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, &text, OBJ_CASE_INSENSITIVE, safeParent.Get(), nullptr);
    IO_STATUS_BLOCK status = {};
    HANDLE raw = nullptr;
    NTSTATUS const result = NtCreateFile(&raw, FILE_GENERIC_WRITE | DELETE | SYNCHRONIZE, &attributes, &status, nullptr, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_CREATE,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT, nullptr, 0);
    if (result < 0)
    {
        FromWin32(error, RtlNtStatusToDosError(result), HostText(host));
        return false;
    }
    JailHandle handle(raw);
    auto const discard = [&handle]
    {
        FILE_DISPOSITION_INFO dispose{ TRUE };
        SetFileInformationByHandle(handle.Get(), FileDispositionInfo, &dispose, sizeof(dispose));
    };
    std::size_t done = 0;
    while (done < contents.size())
    {
        DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(contents.size() - done, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(raw, contents.data() + done, chunk, &written, nullptr) || written == 0)
        {
            DWORD const code = GetLastError();
            discard();
            FromWin32(error, code, HostText(host));
            return false;
        }
        done += written;
    }
    if (!FlushFileBuffers(raw))
    {
        DWORD const code = GetLastError();
        discard();
        FromWin32(error, code, HostText(host));
        return false;
    }
    reservation->Settle();
    return true;
}

bool Ambrose::FileJail::Replace(JailEntry const& entry, std::string_view contents, std::string_view previous, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal)
{
    if (!entry.Parent.Valid() || entry.Leaf.empty() || entry.Stat.Kind != EntryKind::File || entry.Stat.Links > 1)
    {
        error.Set(JailFailure::Refused, "invalid", "Only a single-link file can be replaced");
        return false;
    }
    SpaceRefusal held;
    std::string why;
    std::optional<SpaceReservation> reservation = guard.Reserve(entry.HostPath.parent_path(), contents.size(), held, why);
    if (!reservation)
    {
        if (refusal)
            *refusal = held;
        error.Set(held.Volume.empty() ? JailFailure::Failed : JailFailure::NoSpace, held.Volume.empty() ? "space_unknown" : "space", why, HostText(entry.HostPath));
        return false;
    }
    DWORD code = 0;
    JailStat parentStat;
    if (!Inspect(entry.Parent.Get(), parentStat, code) || parentStat.Kind != EntryKind::Folder)
    {
        FromWin32(error, code, entry.HostPath.parent_path());
        return false;
    }
    JailEntry parentEntry;
    parentEntry.HostPath = entry.HostPath.parent_path();
    parentEntry.Stat = parentStat;
    JailHandle parent(OpenFolderForRename(parentEntry, false, false, false, code));
    if (!parent.Valid())
    {
        FromWin32(error, code, parentEntry.HostPath);
        return false;
    }
    HANDLE const raw = OpenRelative(parent.Get(), Wide(entry.Leaf), GENERIC_READ | GENERIC_WRITE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_OPEN_REPARSE_POINT | FILE_NON_DIRECTORY_FILE, code);
    if (!raw)
    {
        FromWin32(error, code, entry.HostPath);
        return false;
    }
    JailHandle handle(raw);
    JailStat current;
    if (!Inspect(raw, current, code) || current.Kind != EntryKind::File || !current.Identity.Same(entry.Stat.Identity) || current.Links > 1)
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    LARGE_INTEGER position = {};
    if (!SetFilePointerEx(raw, position, nullptr, FILE_BEGIN))
    {
        FromWin32(error, GetLastError(), entry.HostPath);
        return false;
    }
    std::string currentBytes;
    if (current.Size > std::numeric_limits<std::size_t>::max())
    {
        error.Set(JailFailure::Refused, "too_large", "The existing file is too large to replace safely", HostText(entry.HostPath));
        return false;
    }
    currentBytes.resize(static_cast<std::size_t>(current.Size));
    std::size_t read = 0;
    while (read < currentBytes.size())
    {
        DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(currentBytes.size() - read, 1u << 20));
        DWORD received = 0;
        if (!ReadFile(raw, currentBytes.data() + read, chunk, &received, nullptr) || received == 0)
        {
            FromWin32(error, GetLastError(), entry.HostPath);
            return false;
        }
        read += received;
    }
    if (currentBytes != previous)
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    auto const write = [raw](std::string_view bytes) -> DWORD
    {
        LARGE_INTEGER start = {};
        if (!SetFilePointerEx(raw, start, nullptr, FILE_BEGIN) || !SetEndOfFile(raw))
            return GetLastError();
        std::size_t done = 0;
        while (done < bytes.size())
        {
            DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - done, 1u << 20));
            DWORD written = 0;
            if (!WriteFile(raw, bytes.data() + done, chunk, &written, nullptr) || written == 0)
                return GetLastError();
            done += written;
        }
        if (!FlushFileBuffers(raw))
            return GetLastError();
        return 0;
    };
    code = write(contents);
    if (code != 0)
    {
        DWORD const original = code;
        if (DWORD const restored = write(previous); restored != 0)
        {
            error.Set(JailFailure::Failed, "restore_failed", fmt::format("The replacement failed with Windows error {}, and the previous contents could not be restored (Windows error {})", original, restored),
                HostText(entry.HostPath));
            return false;
        }
        FromWin32(error, original, entry.HostPath);
        return false;
    }
    reservation->Settle();
    return true;
}

bool Ambrose::FileJail::Rename(JailEntry const& source, JailEntry const& targetParent, std::string_view name, bool replace, JailError& error)
{
    if (!source.Parent.Valid() || source.Leaf.empty() || !targetParent.Handle.Valid() || targetParent.Stat.Kind != EntryKind::Folder || name.empty())
    {
        error.Set(JailFailure::Failed, "invalid", "Nothing can be renamed from here");
        return false;
    }
    DWORD targetCode = 0;
    JailHandle targetHandle(OpenFolderForRename(targetParent, source.Stat.Kind == EntryKind::Folder, false, true, targetCode));
    if (!targetHandle.Valid())
    {
        FromWin32(error, targetCode, targetParent.HostPath);
        return false;
    }
    HANDLE const existing = OpenRelative(targetHandle.Get(), Wide(name), FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_OPEN_REPARSE_POINT, targetCode);
    if (existing)
    {
        CloseHandle(existing);
        if (!replace)
        {
            error.Set(JailFailure::Conflict, "exists", "An entry already has this name", HostText(targetParent.HostPath / std::filesystem::path(Wide(name))));
            return false;
        }
    }
    else if (targetCode != ERROR_FILE_NOT_FOUND && targetCode != ERROR_PATH_NOT_FOUND)
    {
        FromWin32(error, targetCode, targetParent.HostPath / std::filesystem::path(Wide(name)));
        return false;
    }
    DWORD code = 0;
    JailStat parentStat;
    if (!Inspect(source.Parent.Get(), parentStat, code) || parentStat.Kind != EntryKind::Folder)
    {
        FromWin32(error, code, source.HostPath.parent_path());
        return false;
    }
    JailEntry sourceParentEntry;
    sourceParentEntry.HostPath = source.HostPath.parent_path();
    sourceParentEntry.Stat = parentStat;
    DWORD parentCode = 0;
    JailHandle sourceParent(OpenFolderForRename(sourceParentEntry, false, true, false, parentCode));
    if (!sourceParent.Valid())
    {
        FromWin32(error, parentCode, sourceParentEntry.HostPath);
        return false;
    }
    ULONG const sourceType = source.Stat.Kind == EntryKind::Folder ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;
    HANDLE const raw = OpenRelative(sourceParent.Get(), Wide(source.Leaf), DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_OPEN_REPARSE_POINT | sourceType, code);
    if (!raw)
    {
        FromWin32(error, code, source.HostPath);
        return false;
    }
    JailHandle reopened(raw);
    JailStat now;
    if (!Inspect(raw, now, code) || now.Kind != source.Stat.Kind || !now.Identity.Same(source.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(source.HostPath));
        return false;
    }
    std::wstring const target = Wide(name);
    std::vector<unsigned char> buffer(sizeof(RenameInformation) + target.size() * sizeof(wchar_t));
    RenameInformation* const info = reinterpret_cast<RenameInformation*>(buffer.data());
    info->ReplaceIfExists = replace ? TRUE : FALSE;
    info->RootDirectory = targetHandle.Get();
    info->FileNameLength = static_cast<ULONG>(target.size() * sizeof(wchar_t));
    std::memcpy(info->FileName, target.data(), target.size() * sizeof(wchar_t));
    using SetInformation = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
    HMODULE const ntdll = GetModuleHandleW(L"ntdll.dll");
    SetInformation const setInformation = ntdll ? reinterpret_cast<SetInformation>(GetProcAddress(ntdll, "NtSetInformationFile")) : nullptr;
    if (!setInformation)
    {
        error.Set(JailFailure::Failed, "io", "The operating system does not provide file renames", HostText(targetParent.HostPath));
        return false;
    }
    IO_STATUS_BLOCK status = {};
    NTSTATUS const result = setInformation(raw, &status, info, static_cast<ULONG>(buffer.size()), static_cast<FILE_INFORMATION_CLASS>(10));
    if (result < 0)
    {
        FromWin32(error, RtlNtStatusToDosError(result), targetParent.HostPath / std::filesystem::path(target));
        return false;
    }
    return true;
}

bool Ambrose::FileJail::Remove(JailEntry const& entry, JailError& error)
{
    if (!entry.Parent.Valid() || entry.Leaf.empty())
    {
        error.Set(JailFailure::Failed, "invalid", "The root itself cannot be removed");
        return false;
    }
    DWORD code = 0;
    JailStat parentStat;
    if (!Inspect(entry.Parent.Get(), parentStat, code) || parentStat.Kind != EntryKind::Folder)
    {
        FromWin32(error, code, entry.HostPath.parent_path());
        return false;
    }
    JailEntry parentEntry;
    parentEntry.HostPath = entry.HostPath.parent_path();
    parentEntry.Stat = parentStat;
    JailHandle parent(OpenFolderForRename(parentEntry, false, true, false, code));
    if (!parent.Valid())
    {
        FromWin32(error, code, parentEntry.HostPath);
        return false;
    }
    ULONG const entryType = entry.Stat.Kind == EntryKind::Folder ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;
    HANDLE const raw = OpenRelative(parent.Get(), Wide(entry.Leaf), DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_OPEN_REPARSE_POINT | entryType, code);
    if (!raw)
    {
        FromWin32(error, code, entry.HostPath);
        return false;
    }
    JailHandle reopened(raw);
    JailStat now;
    if (!Inspect(raw, now, code) || now.Kind != entry.Stat.Kind || !now.Identity.Same(entry.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    FILE_DISPOSITION_INFO_EX dispose{ FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS };
    if (!SetFileInformationByHandle(raw, FileDispositionInfoEx, &dispose, sizeof(dispose)))
    {
        FromWin32(error, GetLastError(), entry.HostPath);
        return false;
    }
    return true;
}
bool Ambrose::FileJail::IdentityOf(std::filesystem::path const& path, FileIdentity& identity, std::string& error)
{
    HANDLE const raw = CreateFileW(path.wstring().c_str(), FILE_READ_ATTRIBUTES, ShareAll, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (raw == INVALID_HANDLE_VALUE)
    {
        error = fmt::format("it could not be opened (Windows error {})", GetLastError());
        return false;
    }
    JailHandle handle(raw);
    JailStat seen;
    DWORD code = 0;
    if (!Inspect(raw, seen, code))
    {
        error = fmt::format("it could not be read (Windows error {})", code);
        return false;
    }
    identity = seen.Identity;
    return true;
}

bool Ambrose::FileJail::UsesOpenat2() noexcept
{
    return false;
}

#else

namespace
{
#ifdef O_PATH
    constexpr int PathOnly = O_PATH;
#else
    constexpr int PathOnly = O_RDONLY | O_NONBLOCK;
#endif

    std::string Displayable(std::string_view name)
    {
        if (Utf::IsValidUtf8(name))
            return std::string(name);
        std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(name, Utf::InvalidPolicy::ReplaceWithU_FFFD);
        return wide ? Utf::Utf16ToUtf8(*wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string()) : std::string();
    }

    Ambrose::EntryKind KindOf(mode_t mode) noexcept
    {
        if (S_ISREG(mode))
            return Ambrose::EntryKind::File;
        if (S_ISDIR(mode))
            return Ambrose::EntryKind::Folder;
        if (S_ISLNK(mode))
            return Ambrose::EntryKind::Link;
        if (S_ISFIFO(mode))
            return Ambrose::EntryKind::Fifo;
        if (S_ISCHR(mode) || S_ISBLK(mode))
            return Ambrose::EntryKind::Device;
        if (S_ISSOCK(mode))
            return Ambrose::EntryKind::Socket;
        return Ambrose::EntryKind::Other;
    }

    Ambrose::JailStat StatOf(struct stat const& status) noexcept
    {
        Ambrose::JailStat stat;
        stat.Kind = KindOf(status.st_mode);
        stat.Size = status.st_size > 0 ? static_cast<uint64>(status.st_size) : 0;
#ifdef __APPLE__
        timespec const& modified = status.st_mtimespec;
#else
        timespec const& modified = status.st_mtim;
#endif
        stat.ModifiedEpochMs = static_cast<int64>(modified.tv_sec) * 1000 + static_cast<int64>(modified.tv_nsec) / 1000000;
        stat.Links = static_cast<uint64>(status.st_nlink);
        stat.Identity.Volume = static_cast<uint64>(status.st_dev);
        stat.Identity.Low = static_cast<uint64>(status.st_ino);
        stat.Identity.Known = true;
        return stat;
    }

    void FromErrno(Ambrose::JailError& error, int code, std::string resolved);

    void FromErrno(Ambrose::JailError& error, int code, std::filesystem::path const& resolved)
    {
        FromErrno(error, code, Ambrose::FileJail::HostText(resolved));
    }

    void FromErrno(Ambrose::JailError& error, int code, std::string resolved)
    {
        switch (code)
        {
            case ENOENT:
                error.Set(Ambrose::JailFailure::Missing, "not_found", "Nothing is at this path", std::move(resolved));
                return;
            case ENOTDIR:
                error.Set(Ambrose::JailFailure::Missing, "not_found", std::string(NotFolderMessage), std::move(resolved));
                return;
            case ELOOP:
                error.Set(Ambrose::JailFailure::Refused, "link", std::string(LinkMessage), std::move(resolved));
                return;
            case EEXIST:
                error.Set(Ambrose::JailFailure::Conflict, "exists", "Something is already at this path", std::move(resolved));
                return;
            case ENOTEMPTY:
                error.Set(Ambrose::JailFailure::Conflict, "not_empty", "This folder is not empty", std::move(resolved));
                return;
            case EACCES:
            case EPERM:
                error.Set(Ambrose::JailFailure::Failed, "denied", "The service user may not open this path", std::move(resolved));
                return;
            case ENOSPC:
            case EDQUOT:
                error.Set(Ambrose::JailFailure::NoSpace, "space", "The volume ran out of space", std::move(resolved));
                return;
            default:
                error.Set(Ambrose::JailFailure::Failed, "io", fmt::format("The path could not be opened: {}", std::strerror(code)), std::move(resolved));
                return;
        }
    }

    std::optional<std::string> ReadLink(int folder, std::string const& name)
    {
        std::string target(256, '\0');
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            ssize_t const length = ::readlinkat(folder, name.c_str(), target.data(), target.size());
            if (length < 0)
                return std::nullopt;
            if (static_cast<std::size_t>(length) < target.size())
            {
                target.resize(static_cast<std::size_t>(length));
                return target;
            }
            target.assign(target.size() * 2, '\0');
        }
        return std::nullopt;
    }

    std::string LinkText(std::filesystem::path const& parentHost, std::optional<std::string> const& target, std::filesystem::path const& fallback)
    {
        if (!target)
            return Ambrose::FileJail::HostText(fallback);
        std::filesystem::path const path(*target);
        return Ambrose::FileJail::HostText((path.is_absolute() ? path : parentHost / path).lexically_normal());
    }

    std::filesystem::path HostOf(std::filesystem::path const& root, std::vector<std::string> const& components, std::size_t count)
    {
        std::filesystem::path host = root;
        for (std::size_t index = 0; index < count && index < components.size(); ++index)
            host /= components[index];
        return host;
    }

#ifdef AMBROSE_JAIL_OPENAT2
    int Openat2(int folder, char const* path, uint64 flags, uint64 resolve)
    {
        open_how how = {};
        how.flags = flags;
        how.mode = 0;
        how.resolve = resolve;
        return static_cast<int>(::syscall(SYS_openat2, folder, path, &how, sizeof(how)));
    }

    bool Openat2Works() noexcept
    {
        static bool const works = []
        {
            int const probe = Openat2(AT_FDCWD, "/", O_PATH | O_CLOEXEC, RESOLVE_NO_MAGICLINKS);
            if (probe < 0)
                return false;
            ::close(probe);
            return true;
        }();
        return works;
    }

    std::optional<Ambrose::JailEntry> Beneath(Ambrose::JailRoot const& root, Ambrose::JailPath const& path, Ambrose::JailError& error)
    {
        std::vector<std::string> const& components = path.Components;
        Ambrose::JailHandle parent;
        if (components.size() == 1)
            parent = root.GetHandle().Duplicate();
        else
        {
            std::string const folders = path.Parent().Text();
            int const opened = Openat2(root.GetHandle().Get(), folders.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC, RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS);
            if (opened < 0)
            {
                int const code = errno;
                if (code == ELOOP || code == EXDEV || code == ENOTDIR || code == EAGAIN)
                    return std::nullopt;
                FromErrno(error, code, Ambrose::FileJail::HostText(HostOf(root.GetPath(), components, components.size() - 1)));
                return std::nullopt;
            }
            parent = Ambrose::JailHandle(opened);
        }
        if (!parent.Valid())
        {
            error.Set(Ambrose::JailFailure::Failed, "io", "The root could not be opened again");
            return std::nullopt;
        }
        std::filesystem::path const host = HostOf(root.GetPath(), components, components.size());
        int const leaf = ::openat(parent.Get(), components.back().c_str(), PathOnly | O_NOFOLLOW | O_CLOEXEC);
        if (leaf < 0)
        {
            FromErrno(error, errno, host);
            return std::nullopt;
        }
        Ambrose::JailHandle handle(leaf);
        struct stat status = {};
        if (::fstat(leaf, &status) != 0)
        {
            FromErrno(error, errno, host);
            return std::nullopt;
        }
        Ambrose::JailStat const stat = StatOf(status);
        if (stat.Kind == Ambrose::EntryKind::Link)
        {
            error.Set(Ambrose::JailFailure::Refused, "link", std::string(LinkMessage), LinkText(host.parent_path(), ReadLink(parent.Get(), components.back()), host));
            return std::nullopt;
        }
        if (RefuseSpecial(stat.Kind, error, Ambrose::FileJail::HostText(host)))
            return std::nullopt;
        Ambrose::JailEntry entry;
        entry.Handle = std::move(handle);
        entry.Parent = std::move(parent);
        entry.Leaf = components.back();
        entry.Components = components;
        entry.Stat = stat;
        entry.HostPath = host;
        return entry;
    }
#endif

    std::optional<Ambrose::JailEntry> Walk(Ambrose::JailRoot const& root, Ambrose::JailPath const& path, Ambrose::LinkPolicy links, Ambrose::JailError& error)
    {
        std::deque<std::string> queue(path.Components.begin(), path.Components.end());
        std::vector<std::string> physical;
        Ambrose::JailHandle current = root.GetHandle().Duplicate();
        std::size_t followed = 0;
        while (current.Valid() && !queue.empty())
        {
            std::string const name = queue.front();
            queue.pop_front();
            std::filesystem::path const parentHost = HostOf(root.GetPath(), physical, physical.size());
            std::filesystem::path const host = parentHost / name;
            int const opened = ::openat(current.Get(), name.c_str(), PathOnly | O_NOFOLLOW | O_CLOEXEC);
            if (opened < 0)
            {
                FromErrno(error, errno, host);
                return std::nullopt;
            }
            Ambrose::JailHandle handle(opened);
            struct stat status = {};
            if (::fstat(opened, &status) != 0)
            {
                FromErrno(error, errno, host);
                return std::nullopt;
            }
            Ambrose::JailStat const stat = StatOf(status);
            if (stat.Kind == Ambrose::EntryKind::Link)
            {
                std::optional<std::string> const target = ReadLink(current.Get(), name);
                std::string const resolved = LinkText(parentHost, target, host);
                if (links == Ambrose::LinkPolicy::Refuse || !target)
                {
                    error.Set(Ambrose::JailFailure::Refused, "link", std::string(LinkMessage), resolved);
                    return std::nullopt;
                }
                if (++followed > Ambrose::FileJail::MaxLinksFollowed)
                {
                    error.Set(Ambrose::JailFailure::Refused, "link", "This path follows too many links", resolved);
                    return std::nullopt;
                }
                std::filesystem::path const targetPath(*target);
                std::vector<std::string> expanded;
                if (targetPath.is_absolute())
                {
                    std::filesystem::path const relative = targetPath.lexically_normal().lexically_relative(root.GetPath());
                    if (relative.empty() || relative.begin()->string() == "..")
                    {
                        error.Set(Ambrose::JailFailure::Refused, "outside", std::string(OutsideMessage), resolved);
                        return std::nullopt;
                    }
                    for (std::filesystem::path const& part : relative)
                        if (part.string() != "." && !part.empty())
                            expanded.push_back(part.string());
                }
                else
                {
                    expanded = physical;
                    for (std::filesystem::path const& part : targetPath)
                    {
                        std::string const piece = part.string();
                        if (piece.empty() || piece == ".")
                            continue;
                        if (piece == "..")
                        {
                            if (expanded.empty())
                            {
                                error.Set(Ambrose::JailFailure::Refused, "outside", std::string(OutsideMessage), resolved);
                                return std::nullopt;
                            }
                            expanded.pop_back();
                            continue;
                        }
                        expanded.push_back(piece);
                    }
                }
                for (auto piece = expanded.rbegin(); piece != expanded.rend(); ++piece)
                    queue.push_front(*piece);
                physical.clear();
                current = root.GetHandle().Duplicate();
                continue;
            }
            if (RefuseSpecial(stat.Kind, error, Ambrose::FileJail::HostText(host)))
                return std::nullopt;
            if (!queue.empty())
            {
                if (stat.Kind != Ambrose::EntryKind::Folder)
                {
                    error.Set(Ambrose::JailFailure::Missing, "not_found", std::string(NotFolderMessage), Ambrose::FileJail::HostText(host));
                    return std::nullopt;
                }
                physical.push_back(name);
                current = std::move(handle);
                continue;
            }
            physical.push_back(name);
            Ambrose::JailEntry entry;
            entry.Handle = std::move(handle);
            entry.Parent = std::move(current);
            entry.Leaf = name;
            entry.Components = physical;
            entry.Stat = stat;
            entry.HostPath = host;
            return entry;
        }
        if (!current.Valid())
        {
            error.Set(Ambrose::JailFailure::Failed, "io", "The root could not be opened again");
            return std::nullopt;
        }
        struct stat status = {};
        if (::fstat(current.Get(), &status) != 0)
        {
            FromErrno(error, errno, root.GetPath());
            return std::nullopt;
        }
        Ambrose::JailEntry entry;
        entry.Stat = StatOf(status);
        entry.Handle = std::move(current);
        entry.HostPath = root.GetPath();
        return entry;
    }
}

Ambrose::JailHandle::Native Ambrose::JailHandle::Invalid() noexcept
{
    return -1;
}

bool Ambrose::JailHandle::Valid() const noexcept
{
    return _native >= 0;
}

void Ambrose::JailHandle::Reset() noexcept
{
    if (Valid())
        ::close(_native);
    _native = Invalid();
}

Ambrose::JailHandle Ambrose::JailHandle::Duplicate() const
{
    if (!Valid())
        return JailHandle();
    return JailHandle(::fcntl(_native, F_DUPFD_CLOEXEC, 0));
}

std::shared_ptr<Ambrose::JailRoot> Ambrose::JailRoot::Open(std::filesystem::path const& path, std::string& error)
{
    std::shared_ptr<JailRoot> root(new JailRoot());
    int const opened = ::open(path.c_str(), PathOnly | O_DIRECTORY | O_CLOEXEC);
    if (opened < 0)
    {
        int const code = errno;
        error = code == ENOENT ? std::string("the folder does not exist") : code == ENOTDIR ? std::string("it is not a folder") : fmt::format("the folder could not be opened: {}", std::strerror(code));
        return nullptr;
    }
    root->_handle = JailHandle(opened);
    struct stat status = {};
    if (::fstat(opened, &status) != 0)
    {
        error = fmt::format("the folder could not be read: {}", std::strerror(errno));
        return nullptr;
    }
    std::error_code code;
    std::filesystem::path canonical = std::filesystem::canonical(path, code);
    if (code)
        canonical = std::filesystem::absolute(path, code).lexically_normal();
    root->_path = canonical;
    root->_identity = StatOf(status).Identity;
    return root;
}

std::optional<Ambrose::JailEntry> Ambrose::FileJail::Resolve(JailRoot const& root, JailPath const& path, LinkPolicy links, JailError& error)
{
#ifdef AMBROSE_JAIL_OPENAT2
    if (!path.IsRoot() && links == LinkPolicy::Refuse && Openat2Works())
    {
        std::optional<JailEntry> entry = Beneath(root, path, error);
        if (entry || error.Failure != JailFailure::None)
            return entry;
    }
#endif
    return Walk(root, path, links, error);
}

bool Ambrose::FileJail::List(JailEntry const& folder, std::size_t mostEntries, std::vector<JailListed>& entries, bool& truncated, JailError& error)
{
    truncated = false;
    if (folder.Stat.Kind != EntryKind::Folder || !folder.Handle.Valid())
    {
        error.Set(JailFailure::Failed, "not_a_folder", "Only a folder can be listed");
        return false;
    }
    int const opened = ::openat(folder.Handle.Get(), ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (opened < 0)
    {
        FromErrno(error, errno, folder.HostPath);
        return false;
    }
    struct stat status = {};
    if (::fstat(opened, &status) != 0 || !StatOf(status).Identity.Same(folder.Stat.Identity))
    {
        ::close(opened);
        error.Set(JailFailure::Refused, "changed", "The folder changed while it was being opened, so it was not listed", HostText(folder.HostPath));
        return false;
    }
    DIR* const directory = ::fdopendir(opened);
    if (!directory)
    {
        int const code = errno;
        ::close(opened);
        FromErrno(error, code, HostText(folder.HostPath));
        return false;
    }
    std::unique_ptr<DIR, int (*)(DIR*)> const holder(directory, &::closedir);
    int const descriptor = ::dirfd(directory);
    for (;;)
    {
        errno = 0;
        dirent const* const item = ::readdir(directory);
        if (!item)
        {
            if (errno != 0)
            {
                FromErrno(error, errno, folder.HostPath);
                return false;
            }
            return true;
        }
        std::string_view const name(item->d_name);
        if (name == "." || name == "..")
            continue;
        if (entries.size() >= mostEntries)
        {
            truncated = true;
            return true;
        }
        JailListed listed;
        listed.Name = Displayable(name);
        struct stat child = {};
        if (::fstatat(descriptor, item->d_name, &child, AT_SYMLINK_NOFOLLOW) != 0)
        {
            listed.Openable = false;
            listed.Problem = fmt::format("This entry could not be read: {}", std::strerror(errno));
            entries.push_back(std::move(listed));
            continue;
        }
        JailStat const stat = StatOf(child);
        listed.Kind = stat.Kind;
        listed.Size = stat.Kind == EntryKind::Folder ? 0 : stat.Size;
        listed.ModifiedEpochMs = stat.ModifiedEpochMs;
        listed.Identity = stat.Identity;
        MarkOpenable(listed, name);
        entries.push_back(std::move(listed));
    }
}

bool Ambrose::FileJail::Read(JailEntry const& entry, uint64 offset, std::size_t length, std::string& bytes, JailError& error)
{
    bytes.clear();
    if (entry.Stat.Kind != EntryKind::File || !entry.Parent.Valid() || entry.Leaf.empty())
    {
        error.Set(JailFailure::Failed, "not_a_file", "Only a file can be read");
        return false;
    }
    int const opened = ::openat(entry.Parent.Get(), entry.Leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
    if (opened < 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    JailHandle handle(opened);
    struct stat status = {};
    if (::fstat(opened, &status) != 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    JailStat const now = StatOf(status);
    if (now.Kind != EntryKind::File || !now.Identity.Same(entry.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    bytes.resize(length);
    std::size_t done = 0;
    while (done < length)
    {
        ssize_t const got = ::pread(opened, bytes.data() + done, length - done, static_cast<off_t>(offset + done));
        if (got < 0)
        {
            if (errno == EINTR)
                continue;
            FromErrno(error, errno, entry.HostPath);
            bytes.clear();
            return false;
        }
        if (got == 0)
            break;
        done += static_cast<std::size_t>(got);
    }
    bytes.resize(done);
    return true;
}

bool Ambrose::FileJail::Create(JailRoot const& root, JailPath const& path, std::string_view contents, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal)
{
    if (path.IsRoot())
    {
        error.Set(JailFailure::Failed, "invalid", "A new file needs a name");
        return false;
    }
    std::optional<JailEntry> const parent = Resolve(root, path.Parent(), LinkPolicy::Refuse, error);
    if (!parent)
        return false;
    if (parent->Stat.Kind != EntryKind::Folder)
    {
        error.Set(JailFailure::Missing, "not_found", std::string(NotFolderMessage), HostText(parent->HostPath));
        return false;
    }
    std::filesystem::path const host = parent->HostPath / path.Leaf();
    SpaceRefusal held;
    std::string why;
    std::optional<SpaceReservation> reservation = guard.Reserve(parent->HostPath, contents.size(), held, why);
    if (!reservation)
    {
        if (refusal)
            *refusal = held;
        error.Set(held.Volume.empty() ? JailFailure::Failed : JailFailure::NoSpace, held.Volume.empty() ? "space_unknown" : "space", why, HostText(host));
        return false;
    }
    std::string const leaf = path.Leaf();
    int const opened = ::openat(parent->Handle.Get(), leaf.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_NOCTTY | O_CLOEXEC, 0644);
    if (opened < 0)
    {
        FromErrno(error, errno, host);
        return false;
    }
    JailHandle handle(opened);
    auto const discard = [&parent, &leaf]
    {
        ::unlinkat(parent->Handle.Get(), leaf.c_str(), 0);
    };
    std::size_t done = 0;
    while (done < contents.size())
    {
        ssize_t const written = ::write(opened, contents.data() + done, contents.size() - done);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            int const code = errno;
            discard();
            FromErrno(error, code, HostText(host));
            return false;
        }
        done += static_cast<std::size_t>(written);
    }
    if (::fsync(opened) != 0)
    {
        int const code = errno;
        discard();
        FromErrno(error, code, HostText(host));
        return false;
    }
    reservation->Settle();
    return true;
}

bool Ambrose::FileJail::Replace(JailEntry const& entry, std::string_view contents, std::string_view previous, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal)
{
    if (!entry.Parent.Valid() || entry.Leaf.empty() || entry.Stat.Kind != EntryKind::File || entry.Stat.Links > 1)
    {
        error.Set(JailFailure::Refused, "invalid", "Only a single-link file can be replaced");
        return false;
    }
    SpaceRefusal held;
    std::string why;
    std::optional<SpaceReservation> reservation = guard.Reserve(entry.HostPath.parent_path(), contents.size(), held, why);
    if (!reservation)
    {
        if (refusal)
            *refusal = held;
        error.Set(held.Volume.empty() ? JailFailure::Failed : JailFailure::NoSpace, held.Volume.empty() ? "space_unknown" : "space", why, HostText(entry.HostPath));
        return false;
    }
    int const opened = ::openat(entry.Parent.Get(), entry.Leaf.c_str(), O_RDWR | O_NOFOLLOW | O_NOCTTY | O_CLOEXEC);
    if (opened < 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    JailHandle handle(opened);
    struct stat status = {};
    if (::fstat(opened, &status) != 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    JailStat const current = StatOf(status);
    if (current.Kind != EntryKind::File || !current.Identity.Same(entry.Stat.Identity) || current.Links > 1)
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    if (current.Size > std::numeric_limits<std::size_t>::max())
    {
        error.Set(JailFailure::Refused, "too_large", "The existing file is too large to replace safely", HostText(entry.HostPath));
        return false;
    }
    std::string currentBytes(static_cast<std::size_t>(current.Size), '\0');
    std::size_t read = 0;
    while (read < currentBytes.size())
    {
        ssize_t const received = ::pread(opened, currentBytes.data() + read, currentBytes.size() - read, static_cast<off_t>(read));
        if (received < 0)
        {
            if (errno == EINTR)
                continue;
            FromErrno(error, errno, entry.HostPath);
            return false;
        }
        if (received == 0)
            break;
        read += static_cast<std::size_t>(received);
    }
    currentBytes.resize(read);
    if (currentBytes != previous)
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    auto const write = [opened](std::string_view bytes) -> int
    {
        if (::ftruncate(opened, 0) != 0)
            return errno;
        std::size_t done = 0;
        while (done < bytes.size())
        {
            ssize_t const written = ::pwrite(opened, bytes.data() + done, bytes.size() - done, static_cast<off_t>(done));
            if (written < 0)
            {
                if (errno == EINTR)
                    continue;
                return errno;
            }
            done += static_cast<std::size_t>(written);
        }
        return ::fsync(opened) == 0 ? 0 : errno;
    };
    int const result = write(contents);
    if (result != 0)
    {
        int const restored = write(previous);
        if (restored != 0)
        {
            error.Set(JailFailure::Failed, "restore_failed", fmt::format("The replacement failed: {}; the previous contents could not be restored: {}",
                std::strerror(result), std::strerror(restored)), HostText(entry.HostPath));
            return false;
        }
        FromErrno(error, result, entry.HostPath);
        return false;
    }
    reservation->Settle();
    return true;
}

bool Ambrose::FileJail::Rename(JailEntry const& source, JailEntry const& targetParent, std::string_view name, bool replace, JailError& error)
{
    if (!source.Parent.Valid() || source.Leaf.empty() || !targetParent.Handle.Valid() || targetParent.Stat.Kind != EntryKind::Folder || name.empty())
    {
        error.Set(JailFailure::Failed, "invalid", "Nothing can be renamed from here");
        return false;
    }
    struct stat status = {};
    if (::fstatat(source.Parent.Get(), source.Leaf.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0)
    {
        FromErrno(error, errno, source.HostPath);
        return false;
    }
    JailStat const now = StatOf(status);
    if (now.Kind != source.Stat.Kind || !now.Identity.Same(source.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(source.HostPath));
        return false;
    }
    std::string const target(name);
    std::filesystem::path const host = targetParent.HostPath / target;
    int result = 0;
#if defined(__linux__) && defined(SYS_renameat2)
    result = static_cast<int>(::syscall(SYS_renameat2, source.Parent.Get(), source.Leaf.c_str(), targetParent.Handle.Get(), target.c_str(), replace ? 0u : 1u));
    if (result != 0 && errno == ENOSYS)
        result = -2;
#else
    result = -2;
#endif
    if (result == -2)
    {
        struct stat existing = {};
        if (!replace && ::fstatat(targetParent.Handle.Get(), target.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0)
        {
            error.Set(JailFailure::Conflict, "exists", "Something is already at this path", HostText(host));
            return false;
        }
        if (!replace)
        {
            error.Set(JailFailure::Failed, "unsupported", "This platform cannot safely rename without replacement", HostText(host));
            return false;
        }
        result = ::renameat(source.Parent.Get(), source.Leaf.c_str(), targetParent.Handle.Get(), target.c_str());
    }
    if (result != 0)
    {
        FromErrno(error, errno, host);
        return false;
    }
    return true;
}

bool Ambrose::FileJail::Remove(JailEntry const& entry, JailError& error)
{
    if (!entry.Parent.Valid() || entry.Leaf.empty())
    {
        error.Set(JailFailure::Failed, "invalid", "The root itself cannot be removed");
        return false;
    }
    struct stat status = {};
    if (::fstatat(entry.Parent.Get(), entry.Leaf.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    JailStat const now = StatOf(status);
    if (now.Kind != entry.Stat.Kind || !now.Identity.Same(entry.Stat.Identity))
    {
        error.Set(JailFailure::Refused, "changed", std::string(ChangedMessage), HostText(entry.HostPath));
        return false;
    }
    if (::unlinkat(entry.Parent.Get(), entry.Leaf.c_str(), entry.Stat.Kind == EntryKind::Folder ? AT_REMOVEDIR : 0) != 0)
    {
        FromErrno(error, errno, entry.HostPath);
        return false;
    }
    return true;
}
bool Ambrose::FileJail::IdentityOf(std::filesystem::path const& path, FileIdentity& identity, std::string& error)
{
    struct stat status = {};
    if (::stat(path.c_str(), &status) != 0)
    {
        error = fmt::format("it could not be read: {}", std::strerror(errno));
        return false;
    }
    identity = StatOf(status).Identity;
    return true;
}

bool Ambrose::FileJail::UsesOpenat2() noexcept
{
#ifdef AMBROSE_JAIL_OPENAT2
    return Openat2Works();
#else
    return false;
#endif
}

#endif
