/*
 * Project Ambrose by Imjustchico
 * Reads key presses from a Windows console handle or a POSIX terminal put in raw mode, turns them into editor keys, and keeps finished lines queued until the reader thread asks for them.
 */

#include "TerminalConsoleInput.h"

#include <algorithm>
#include <iterator>
#include <thread>

#ifdef _WIN32
#include "Utf.h"

#include <optional>

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
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
    constexpr std::chrono::milliseconds MaxWait{ 60000 };

    long WaitMilliseconds(std::chrono::milliseconds timeout)
    {
        return static_cast<long>(std::clamp<std::chrono::milliseconds::rep>(timeout.count(), 0, MaxWait.count()));
    }
}

#ifdef _WIN32

struct TerminalConsoleInput::State
{
    HANDLE Handle = nullptr;
    char16_t HighSurrogate = 0;
};

namespace
{
    ConsoleKeyKind KindForVirtualKey(WORD key, bool control)
    {
        switch (key)
        {
            case VK_RETURN: return ConsoleKeyKind::Enter;
            case VK_BACK: return control ? ConsoleKeyKind::DeleteWord : ConsoleKeyKind::Backspace;
            case VK_TAB: return ConsoleKeyKind::Tab;
            case VK_DELETE: return ConsoleKeyKind::Delete;
            case VK_LEFT: return control ? ConsoleKeyKind::WordLeft : ConsoleKeyKind::Left;
            case VK_RIGHT: return control ? ConsoleKeyKind::WordRight : ConsoleKeyKind::Right;
            case VK_UP: return ConsoleKeyKind::Up;
            case VK_DOWN: return ConsoleKeyKind::Down;
            case VK_HOME: return ConsoleKeyKind::Home;
            case VK_END: return ConsoleKeyKind::End;
            case VK_ESCAPE: return ConsoleKeyKind::ClearLine;
            default: break;
        }
        return ConsoleKeyKind::None;
    }

    ConsoleKeyKind KindForControlCharacter(wchar_t character)
    {
        switch (character)
        {
            case 0x01: return ConsoleKeyKind::Home;
            case 0x02: return ConsoleKeyKind::Left;
            case 0x03: return ConsoleKeyKind::Interrupt;
            case 0x04: return ConsoleKeyKind::EndOfFile;
            case 0x05: return ConsoleKeyKind::End;
            case 0x06: return ConsoleKeyKind::Right;
            case 0x0B: return ConsoleKeyKind::KillToEnd;
            case 0x15: return ConsoleKeyKind::ClearLine;
            case 0x17: return ConsoleKeyKind::DeleteWord;
            default: break;
        }
        return ConsoleKeyKind::None;
    }
}

TerminalConsoleInput::TerminalConsoleInput(ConsoleWriter& console, std::string prompt, ConsoleLineEditor::Completer completer)
    : _prompt(console, std::move(prompt)), _state(std::make_unique<State>())
{
    _prompt.Editor().SetCompleter(std::move(completer));
    HANDLE const handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        _closed = true;
    else
    {
        _state->Handle = handle;
        _prompt.Attach();
    }
}

TerminalConsoleInput::~TerminalConsoleInput()
{
    _prompt.Detach();
}

bool TerminalConsoleInput::IsAvailable(ConsoleWriter& console)
{
    if (!console.IsTerminal())
        return false;
    HANDLE const handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        return false;
    DWORD mode = 0;
    return GetConsoleMode(handle, &mode) != 0;
}

bool TerminalConsoleInput::ReadKeys(std::vector<ConsoleKey>& keys, std::chrono::milliseconds timeout)
{
    if (_state->Handle == nullptr)
        return false;
    DWORD const waited = WaitForSingleObject(_state->Handle, static_cast<DWORD>(WaitMilliseconds(timeout)));
    if (waited == WAIT_TIMEOUT)
        return true;
    if (waited != WAIT_OBJECT_0)
        return false;
    INPUT_RECORD records[64];
    DWORD count = 0;
    if (!ReadConsoleInputW(_state->Handle, records, static_cast<DWORD>(std::size(records)), &count))
        return false;
    for (DWORD index = 0; index < count; ++index)
    {
        if (records[index].EventType != KEY_EVENT || records[index].Event.KeyEvent.bKeyDown == FALSE)
            continue;
        KEY_EVENT_RECORD const& event = records[index].Event.KeyEvent;
        bool const control = (event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        ConsoleKey key;
        key.Kind = KindForVirtualKey(event.wVirtualKeyCode, control);
        if (key.Kind == ConsoleKeyKind::None)
        {
            wchar_t const character = event.uChar.UnicodeChar;
            if (character == 0)
                continue;
            if (character < 0x20 || character == 0x7F)
            {
                key.Kind = KindForControlCharacter(character);
                if (key.Kind == ConsoleKeyKind::None)
                    continue;
            }
            else if (character >= 0xD800 && character <= 0xDBFF)
            {
                _state->HighSurrogate = static_cast<char16_t>(character);
                continue;
            }
            else
            {
                std::u16string wide;
                if (character >= 0xDC00 && character <= 0xDFFF)
                {
                    if (_state->HighSurrogate == 0)
                        continue;
                    wide.push_back(_state->HighSurrogate);
                }
                wide.push_back(static_cast<char16_t>(character));
                _state->HighSurrogate = 0;
                std::optional<std::string> const text = Utf::Utf16ToUtf8(wide, Utf::InvalidPolicy::ReplaceWithU_FFFD);
                if (!text)
                    continue;
                key.Kind = ConsoleKeyKind::Character;
                key.Text = *text;
            }
        }
        for (WORD repeat = 0; repeat < event.wRepeatCount && repeat < 32; ++repeat)
            keys.push_back(key);
    }
    return true;
}

void TerminalConsoleInput::Interrupt()
{
    _interrupted = true;
}

#else

struct TerminalConsoleInput::State
{
    termios Saved{};
    bool Raw = false;
};

namespace
{
    bool IsForeground()
    {
        pid_t const foreground = ::tcgetpgrp(STDIN_FILENO);
        return foreground < 0 || foreground == ::getpgrp();
    }
}

TerminalConsoleInput::TerminalConsoleInput(ConsoleWriter& console, std::string prompt, ConsoleLineEditor::Completer completer)
    : _prompt(console, std::move(prompt)), _state(std::make_unique<State>())
{
    _prompt.Editor().SetCompleter(std::move(completer));
    if (IsForeground() && ::tcgetattr(STDIN_FILENO, &_state->Saved) == 0)
    {
        termios raw = _state->Saved;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        _state->Raw = ::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }
    if (!_state->Raw)
        _closed = true;
    else
        _prompt.Attach();
}

TerminalConsoleInput::~TerminalConsoleInput()
{
    _prompt.Detach();
    if (_state->Raw)
        ::tcsetattr(STDIN_FILENO, TCSANOW, &_state->Saved);
}

bool TerminalConsoleInput::IsAvailable(ConsoleWriter& console)
{
    return console.IsTerminal() && ::isatty(STDIN_FILENO) == 1;
}

bool TerminalConsoleInput::ReadKeys(std::vector<ConsoleKey>& keys, std::chrono::milliseconds timeout)
{
    if (!IsForeground())
    {
        std::this_thread::sleep_for(timeout);
        return true;
    }
    pollfd descriptor{ STDIN_FILENO, POLLIN, 0 };
    int const ready = ::poll(&descriptor, 1, static_cast<int>(WaitMilliseconds(timeout)));
    if (ready < 0)
        return errno == EINTR;
    if (ready == 0)
    {
        _decoder.Flush();
        return true;
    }
    if ((descriptor.revents & POLLNVAL) != 0)
        return false;
    char chunk[512];
    ssize_t const count = ::read(STDIN_FILENO, chunk, sizeof(chunk));
    if (count < 0)
        return errno == EINTR || errno == EAGAIN;
    if (count == 0)
        return false;
    _decoder.Feed(std::string_view(chunk, static_cast<std::size_t>(count)));
    ConsoleKey key;
    while (_decoder.Next(key))
        keys.push_back(key);
    return true;
}

void TerminalConsoleInput::Interrupt()
{
    _interrupted = true;
}

#endif

ConsoleInput::ReadResult TerminalConsoleInput::ReadLine(std::string& line, std::chrono::milliseconds timeout)
{
    if (!_lines.empty())
    {
        line = std::move(_lines.front());
        _lines.pop_front();
        return ReadResult::Line;
    }
    if (_interrupted.load() || _closed.load())
        return ReadResult::Closed;

    std::vector<ConsoleKey> keys;
    if (!ReadKeys(keys, timeout))
    {
        _closed = true;
        return ReadResult::Closed;
    }
    if (_interrupted.load())
        return ReadResult::Closed;

    for (ConsoleKey const& key : keys)
    {
        std::string finished;
        ConsolePrompt::Result const result = _prompt.Apply(key, finished);
        if (result == ConsolePrompt::Result::Line)
        {
            if (_lines.size() < MaxPendingLines)
                _lines.push_back(std::move(finished));
        }
        else if (result == ConsolePrompt::Result::Closed)
        {
            _closed = true;
            break;
        }
    }
    if (!_lines.empty())
    {
        line = std::move(_lines.front());
        _lines.pop_front();
        return ReadResult::Line;
    }
    return _closed.load() ? ReadResult::Closed : ReadResult::Timeout;
}
