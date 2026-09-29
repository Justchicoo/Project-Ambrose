/*
 * Project Ambrose by Imjustchico
 * Makes each kind of link and special file with the system's own calls: a junction on Windows is a folder given a mount point reparse buffer naming its target with the \??\ prefix and a print name, a unix socket file is a socket bound to the path and closed, which leaves the file behind, and a named pipe is mkfifo; whatever the system cannot make is refused with the reason, so the test that wanted it can say why it skipped.
 */

#include "FileLinks.h"

#include <fmt/format.h>

#include <cstring>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <afunix.h>
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#endif

bool FileLinks::MakeFolderLink(std::filesystem::path const& link, std::filesystem::path const& target, std::string& why)
{
    std::error_code code;
#ifdef _WIN32
    std::filesystem::create_directory(link, code);
    if (code)
    {
        why = fmt::format("the junction's folder could not be made: {}", code.message());
        return false;
    }
    HANDLE const handle = CreateFileW(link.wstring().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        why = fmt::format("the junction's folder could not be opened (Windows error {})", GetLastError());
        return false;
    }
    std::wstring const print = std::filesystem::absolute(target, code).lexically_normal().wstring();
    std::wstring const substitute = L"\\??\\" + print;
    std::size_t const substituteBytes = substitute.size() * sizeof(wchar_t);
    std::size_t const printBytes = print.size() * sizeof(wchar_t);
    std::size_t const pathBytes = substituteBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t);
    std::vector<unsigned char> buffer(16 + pathBytes, 0);
    auto const put = [&buffer](std::size_t at, std::size_t value, std::size_t width)
    {
        DWORD const wide = static_cast<DWORD>(value);
        USHORT const narrow = static_cast<USHORT>(value);
        std::memcpy(buffer.data() + at, width == 4 ? static_cast<void const*>(&wide) : static_cast<void const*>(&narrow), width);
    };
    put(0, IO_REPARSE_TAG_MOUNT_POINT, 4);
    put(4, 8 + pathBytes, 2);
    put(8, 0, 2);
    put(10, substituteBytes, 2);
    put(12, substituteBytes + sizeof(wchar_t), 2);
    put(14, printBytes, 2);
    std::memcpy(buffer.data() + 16, substitute.data(), substituteBytes);
    std::memcpy(buffer.data() + 16 + substituteBytes + sizeof(wchar_t), print.data(), printBytes);
    DWORD returned = 0;
    BOOL const written = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr);
    DWORD const failure = GetLastError();
    CloseHandle(handle);
    if (!written)
    {
        why = fmt::format("the junction could not be written (Windows error {})", failure);
        return false;
    }
    return true;
#else
    std::filesystem::create_directory_symlink(target, link, code);
    if (code)
    {
        why = fmt::format("the folder link could not be made: {}", code.message());
        return false;
    }
    return true;
#endif
}

bool FileLinks::MakeFileLink(std::filesystem::path const& link, std::filesystem::path const& target, std::string& why)
{
    std::error_code code;
    std::filesystem::create_symlink(target, link, code);
    if (code)
    {
        why = fmt::format("this user may not make a symbolic link here ({}); on Windows that takes Developer Mode or the privilege to create symbolic links", code.message());
        return false;
    }
    return true;
}

bool FileLinks::MakeFifo(std::filesystem::path const& path, std::string& why)
{
#ifdef _WIN32
    why = fmt::format("an NTFS folder cannot hold a named pipe, so {} was not made", path.filename().string());
    return false;
#else
    if (::mkfifo(path.c_str(), 0600) != 0)
    {
        why = fmt::format("the named pipe could not be made: {}", std::strerror(errno));
        return false;
    }
    return true;
#endif
}

bool FileLinks::MakeSocket(std::filesystem::path const& path, std::string& why)
{
    std::string const text = path.string();
#ifdef _WIN32
    WSADATA data = {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        why = "Winsock did not start";
        return false;
    }
    SOCKET const handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle == INVALID_SOCKET)
    {
        why = fmt::format("this Windows makes no AF_UNIX socket (Winsock error {}); they arrived with Windows 10 version 1803", WSAGetLastError());
        WSACleanup();
        return false;
    }
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    if (text.size() >= sizeof(address.sun_path))
    {
        why = "the socket's path is longer than an AF_UNIX address holds";
        closesocket(handle);
        WSACleanup();
        return false;
    }
    std::memcpy(address.sun_path, text.c_str(), text.size());
    int const bound = ::bind(handle, reinterpret_cast<sockaddr const*>(&address), static_cast<int>(sizeof(address)));
    int const failure = WSAGetLastError();
    closesocket(handle);
    WSACleanup();
    if (bound != 0)
    {
        why = fmt::format("the socket could not be bound (Winsock error {})", failure);
        return false;
    }
    return true;
#else
    int const handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle < 0)
    {
        why = fmt::format("no unix socket could be made: {}", std::strerror(errno));
        return false;
    }
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    if (text.size() >= sizeof(address.sun_path))
    {
        why = "the socket's path is longer than an AF_UNIX address holds";
        ::close(handle);
        return false;
    }
    std::memcpy(address.sun_path, text.c_str(), text.size());
    int const bound = ::bind(handle, reinterpret_cast<sockaddr const*>(&address), sizeof(address));
    int const failure = errno;
    ::close(handle);
    if (bound != 0)
    {
        why = fmt::format("the socket could not be bound: {}", std::strerror(failure));
        return false;
    }
    return true;
#endif
}
