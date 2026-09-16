/*
 * Project Ambrose by Imjustchico
 * Reads standard input line by line: ReadConsoleW, pipe polling or file reads on Windows, where an injected Enter or cancelled IO ends a pending read, and poll on POSIX, skipping reads only while a terminal names another foreground job.
 */

#include "ConsoleInput.h"
#include "Log.h"
#include "Utf.h"

#include <algorithm>
#include <iterator>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif

void StandardConsoleInput::Append(char const* data, std::size_t size)
{
    if (_skipToNewline)
    {
        std::string_view const incoming(data, size);
        std::size_t const newline = incoming.find('\n');
        if (newline == std::string_view::npos)
            return;
        _skipToNewline = false;
        data += newline + 1;
        size -= newline + 1;
    }
    _buffer.append(data, size);
    if (_buffer.size() > MaxPendingLine && _buffer.find('\n') == std::string::npos)
    {
        LOG_WARN("commands.console", "Discarded a console line of over {} bytes that had no end", MaxPendingLine);
        _buffer.clear();
        _skipToNewline = true;
    }
}

bool StandardConsoleInput::TakeBufferedLine(std::string& line)
{
    std::size_t const end = _buffer.find('\n');
    if (end == std::string::npos)
        return false;
    line.assign(_buffer, 0, end);
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    _buffer.erase(0, end + 1);
    return true;
}

#ifdef _WIN32

StandardConsoleInput::StandardConsoleInput()
{
    HANDLE const handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
    {
        _closed = true;
        return;
    }
    _handle = handle;
    _fileType = GetFileType(handle);
    DWORD mode = 0;
    if (_fileType == FILE_TYPE_CHAR && !GetConsoleMode(handle, &mode))
        _fileType = FILE_TYPE_UNKNOWN;
}

StandardConsoleInput::~StandardConsoleInput() = default;

ConsoleInput::ReadResult StandardConsoleInput::ReadLine(std::string& line, std::chrono::milliseconds timeout)
{
    if (TakeBufferedLine(line))
        return ReadResult::Line;
    if (_closed.load() || _interrupted.load())
        return ReadResult::Closed;
    if (_fileType == FILE_TYPE_CHAR)
        return ReadConsoleLine(line);
    if (_fileType == FILE_TYPE_PIPE)
        return ReadPipe(line, timeout);

    char chunk[4096];
    DWORD read = 0;
    if (!ReadFile(static_cast<HANDLE>(_handle), chunk, sizeof(chunk), &read, nullptr) || read == 0)
    {
        _closed = true;
        if (_buffer.empty())
            return ReadResult::Closed;
        line = std::move(_buffer);
        _buffer.clear();
        return ReadResult::Line;
    }
    Append(chunk, read);
    return TakeBufferedLine(line) ? ReadResult::Line : ReadResult::Timeout;
}

ConsoleInput::ReadResult StandardConsoleInput::ReadConsoleLine(std::string& line)
{
    auto const releaseThread = [this]
    {
        std::lock_guard const lock(_threadMutex);
        if (_readerThread)
            CloseHandle(static_cast<HANDLE>(_readerThread));
        _readerThread = nullptr;
    };
    {
        std::lock_guard const lock(_threadMutex);
        HANDLE thread = nullptr;
        if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread, 0, FALSE, DUPLICATE_SAME_ACCESS))
            _readerThread = thread;
    }
    if (_interrupted.load())
    {
        releaseThread();
        return ReadResult::Closed;
    }

    wchar_t chunk[1024];
    DWORD read = 0;
    BOOL const ok = ReadConsoleW(static_cast<HANDLE>(_handle), chunk, static_cast<DWORD>(std::size(chunk)), &read, nullptr);
    DWORD const error = ok ? ERROR_SUCCESS : GetLastError();
    releaseThread();

    if (_interrupted.load())
        return ReadResult::Closed;
    if (!ok)
    {
        if (error == ERROR_OPERATION_ABORTED)
            return ReadResult::Timeout;
        _closed = true;
        return ReadResult::Closed;
    }
    if (read == 0)
        return ReadResult::Timeout;
    std::optional<std::string> const text = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(chunk), read), Utf::InvalidPolicy::ReplaceWithU_FFFD);
    if (text)
        Append(text->data(), text->size());
    return TakeBufferedLine(line) ? ReadResult::Line : ReadResult::Timeout;
}

ConsoleInput::ReadResult StandardConsoleInput::ReadPipe(std::string& line, std::chrono::milliseconds timeout)
{
    auto const deadline = std::chrono::steady_clock::now() + timeout;
    for (;;)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(_handle), nullptr, 0, nullptr, &available, nullptr))
        {
            _closed = true;
            if (_buffer.empty())
                return ReadResult::Closed;
            line = std::move(_buffer);
            _buffer.clear();
            return ReadResult::Line;
        }
        if (available > 0)
        {
            char chunk[4096];
            DWORD read = 0;
            if (!ReadFile(static_cast<HANDLE>(_handle), chunk, std::min<DWORD>(available, sizeof(chunk)), &read, nullptr))
            {
                _closed = true;
                return ReadResult::Closed;
            }
            Append(chunk, read);
            if (TakeBufferedLine(line))
                return ReadResult::Line;
            continue;
        }
        if (_interrupted.load())
            return ReadResult::Closed;
        if (std::chrono::steady_clock::now() >= deadline)
            return ReadResult::Timeout;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void StandardConsoleInput::Interrupt()
{
    _interrupted = true;
    std::lock_guard const lock(_threadMutex);
    if (_fileType == FILE_TYPE_CHAR && _readerThread)
    {
        INPUT_RECORD records[2]{};
        for (INPUT_RECORD& record : records)
        {
            record.EventType = KEY_EVENT;
            record.Event.KeyEvent.wRepeatCount = 1;
            record.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
            record.Event.KeyEvent.uChar.UnicodeChar = L'\r';
        }
        records[0].Event.KeyEvent.bKeyDown = TRUE;
        DWORD written = 0;
        WriteConsoleInputW(static_cast<HANDLE>(_handle), records, 2, &written);
    }
    if (_readerThread)
        CancelSynchronousIo(static_cast<HANDLE>(_readerThread));
}

#else

StandardConsoleInput::StandardConsoleInput() = default;

StandardConsoleInput::~StandardConsoleInput() = default;

ConsoleInput::ReadResult StandardConsoleInput::ReadLine(std::string& line, std::chrono::milliseconds timeout)
{
    if (TakeBufferedLine(line))
        return ReadResult::Line;
    if (_closed.load() || _interrupted.load())
        return ReadResult::Closed;

    if (isatty(STDIN_FILENO))
    {
        pid_t const foreground = tcgetpgrp(STDIN_FILENO);
        if (foreground >= 0 && foreground != getpgrp())
        {
            std::this_thread::sleep_for(timeout);
            return _interrupted.load() ? ReadResult::Closed : ReadResult::Timeout;
        }
    }

    pollfd descriptor{ STDIN_FILENO, POLLIN, 0 };
    int const ready = poll(&descriptor, 1, static_cast<int>(std::clamp<std::chrono::milliseconds::rep>(timeout.count(), 0, 60000)));
    if (_interrupted.load())
        return ReadResult::Closed;
    if (ready < 0)
    {
        if (errno == EINTR)
            return ReadResult::Timeout;
        _closed = true;
        return ReadResult::Closed;
    }
    if (ready == 0)
        return ReadResult::Timeout;
    if ((descriptor.revents & POLLNVAL) != 0)
    {
        _closed = true;
        return ReadResult::Closed;
    }

    char chunk[4096];
    ssize_t const count = ::read(STDIN_FILENO, chunk, sizeof(chunk));
    if (count < 0)
    {
        if (errno == EINTR || errno == EAGAIN)
            return ReadResult::Timeout;
        _closed = true;
        return ReadResult::Closed;
    }
    if (count == 0)
    {
        _closed = true;
        if (_buffer.empty())
            return ReadResult::Closed;
        line = std::move(_buffer);
        _buffer.clear();
        return ReadResult::Line;
    }
    Append(chunk, static_cast<std::size_t>(count));
    return TakeBufferedLine(line) ? ReadResult::Line : ReadResult::Timeout;
}

void StandardConsoleInput::Interrupt()
{
    _interrupted = true;
}

#endif
