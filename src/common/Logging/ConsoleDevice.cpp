/*
 * Project Ambrose by Imjustchico
 * Writes UTF-8 to the process standard output as a Windows console, a mintty pipe, a POSIX terminal, or a redirect.
 */

#include "ConsoleDevice.h"
#include "Utf.h"

#include <atomic>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

namespace
{
#ifdef _WIN32
    bool IsMinttyPipe(HANDLE handle)
    {
        if (::GetFileType(handle) != FILE_TYPE_PIPE)
            return false;
        std::size_t const size = sizeof(FILE_NAME_INFO) + sizeof(WCHAR) * MAX_PATH;
        std::unique_ptr<std::byte[]> buffer(new std::byte[size]());
        FILE_NAME_INFO* const info = reinterpret_cast<FILE_NAME_INFO*>(buffer.get());
        if (!::GetFileInformationByHandleEx(handle, FileNameInfo, info, static_cast<DWORD>(size - sizeof(WCHAR))))
            return false;
        std::wstring const name(info->FileName, info->FileNameLength / sizeof(WCHAR));
        bool const msys = name.find(L"msys-") != std::wstring::npos || name.find(L"cygwin-") != std::wstring::npos;
        return msys && name.find(L"-pty") != std::wstring::npos;
    }

    WORD LegacyAttribute(ConsoleColor color)
    {
        static constexpr WORD const Table[] = {
            0,
            FOREGROUND_RED,
            FOREGROUND_GREEN,
            FOREGROUND_RED | FOREGROUND_GREEN,
            FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_BLUE,
            FOREGROUND_GREEN | FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_INTENSITY,
            FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
        };
        std::size_t const index = static_cast<std::size_t>(color);
        return index < std::size(Table) ? Table[index] : 0;
    }

    class StandardOutputDevice final : public ConsoleDevice
    {
    public:
        StandardOutputDevice()
        {
            _handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
            if (_handle == nullptr || _handle == INVALID_HANDLE_VALUE)
            {
                _handle = nullptr;
                return;
            }
            DWORD mode = 0;
            if (::GetConsoleMode(_handle, &mode))
            {
                _console = true;
                _terminal = true;
                CONSOLE_SCREEN_BUFFER_INFO info{};
                if (::GetConsoleScreenBufferInfo(_handle, &info))
                    _defaultAttributes = info.wAttributes;
                if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
                    _virtualTerminal = true;
                else if (::SetConsoleMode(_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                {
                    _virtualTerminal = true;
                    _savedMode = mode;
                    _modeChanged = true;
                }
                return;
            }
            if (IsMinttyPipe(_handle))
            {
                _terminal = true;
                _virtualTerminal = true;
            }
        }

        bool IsTerminal() const noexcept override { return _terminal; }
        bool SupportsVirtualTerminal() const noexcept override { return _virtualTerminal; }

        void Write(std::string_view utf8) override
        {
            if (_handle == nullptr || utf8.empty())
                return;
            if (_console)
            {
                std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(utf8, Utf::InvalidPolicy::ReplaceWithU_FFFD);
                if (!wide)
                    return;
                wchar_t const* data = reinterpret_cast<wchar_t const*>(wide->data());
                std::size_t remaining = wide->size();
                while (remaining > 0)
                {
                    DWORD const chunk = static_cast<DWORD>(remaining > 16384 ? 16384 : remaining);
                    DWORD written = 0;
                    if (!::WriteConsoleW(_handle, data, chunk, &written, nullptr) || written == 0)
                        return;
                    data += written;
                    remaining -= written;
                }
                return;
            }
            char const* data = utf8.data();
            std::size_t remaining = utf8.size();
            while (remaining > 0)
            {
                DWORD const chunk = static_cast<DWORD>(remaining > 1048576 ? 1048576 : remaining);
                DWORD written = 0;
                if (!::WriteFile(_handle, data, chunk, &written, nullptr) || written == 0)
                    return;
                data += written;
                remaining -= written;
            }
        }

        void SetLegacyColor(ConsoleColor color) override
        {
            if (!_console || color == ConsoleColor::Default)
                return;
            WORD const background = _defaultAttributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
            ::SetConsoleTextAttribute(_handle, static_cast<WORD>(LegacyAttribute(color) | background));
        }

        void ResetLegacyColor() override
        {
            if (_console)
                ::SetConsoleTextAttribute(_handle, _defaultAttributes);
        }

        void Flush() override
        {
        }

        void Restore() override
        {
            if (_modeChanged)
            {
                ::SetConsoleMode(_handle, _savedMode);
                _modeChanged = false;
            }
        }

    private:
        HANDLE _handle = nullptr;
        bool _console = false;
        bool _terminal = false;
        bool _virtualTerminal = false;
        bool _modeChanged = false;
        DWORD _savedMode = 0;
        WORD _defaultAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    };
#else
    class StandardOutputDevice final : public ConsoleDevice
    {
    public:
        StandardOutputDevice()
        {
            char const* const term = std::getenv("TERM");
            _terminal = ::isatty(STDOUT_FILENO) == 1 && !(term != nullptr && std::string_view(term) == "dumb");
        }

        bool IsTerminal() const noexcept override { return _terminal; }
        bool SupportsVirtualTerminal() const noexcept override { return _terminal; }

        void Write(std::string_view utf8) override
        {
            char const* data = utf8.data();
            std::size_t remaining = utf8.size();
            while (remaining > 0)
            {
                ssize_t const written = ::write(STDOUT_FILENO, data, remaining);
                if (written < 0)
                {
                    if (errno == EINTR)
                        continue;
                    _errors.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                data += written;
                remaining -= static_cast<std::size_t>(written);
            }
        }

        void SetLegacyColor(ConsoleColor) override
        {
        }

        void ResetLegacyColor() override
        {
        }

        void Flush() override
        {
        }

        void Restore() override
        {
        }

    private:
        bool _terminal = false;
        std::atomic<uint64> _errors{ 0 };
    };
#endif
}

std::unique_ptr<ConsoleDevice> ConsoleDevice::CreateStandardOutput()
{
    return std::make_unique<StandardOutputDevice>();
}
