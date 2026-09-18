/*
 * Project Ambrose by Imjustchico
 * Keeps the prompt row correct under the console writer's lock, which every edit and every log line takes: scrolling a line too long for the window sideways so it never wraps onto a second row, erasing the row before a log line, drawing it again after one, leaving a submitted line behind as scrollback, and listing completions above a fresh prompt.
 */

#include "ConsolePrompt.h"

#include <utility>

ConsolePrompt::ConsolePrompt(ConsoleWriter& console, std::string text) : _console(console), _text(std::move(text))
{
}

ConsolePrompt::~ConsolePrompt()
{
    Detach();
}

ConsoleLineEditor& ConsolePrompt::Editor() noexcept
{
    return _editor;
}

void ConsolePrompt::Attach()
{
    if (_attached || !_console.IsTerminal())
        return;
    _console.SetLineHooks([this](ConsoleDevice& device) { EraseLocked(device); }, [this](ConsoleDevice& device) { DrawLocked(device, Draw::Cursor); });
    _console.WithLock([this](ConsoleDevice& device)
    {
        _attached = true;
        DrawLocked(device, Draw::Cursor);
        device.Flush();
    });
}

void ConsolePrompt::Detach()
{
    if (!_attached)
        return;
    _console.WithLock([this](ConsoleDevice& device)
    {
        EraseLocked(device);
        _attached = false;
        device.Flush();
    });
    _console.SetLineHooks(nullptr, nullptr);
}

ConsolePrompt::Result ConsolePrompt::Apply(ConsoleKey const& key, std::string& line)
{
    Result result = Result::Pending;
    _console.WithLock([this, &key, &line, &result](ConsoleDevice& device)
    {
        switch (_editor.Apply(key))
        {
            case ConsoleLineEditor::Action::Redraw:
                EraseLocked(device);
                DrawLocked(device, Draw::Cursor);
                device.Flush();
                break;
            case ConsoleLineEditor::Action::Suggest:
            {
                std::string text;
                for (std::string const& suggestion : _editor.GetSuggestions())
                {
                    text.append(suggestion);
                    text.push_back('\n');
                }
                _console.WriteLines(text, ConsoleColor::Default);
                break;
            }
            case ConsoleLineEditor::Action::Submit:
                CommitLocked(device, {});
                line = _editor.TakeLine();
                DrawLocked(device, Draw::Cursor);
                device.Flush();
                result = Result::Line;
                break;
            case ConsoleLineEditor::Action::Interrupt:
                CommitLocked(device, "^C");
                _editor.Clear();
                DrawLocked(device, Draw::Cursor);
                device.Flush();
                break;
            case ConsoleLineEditor::Action::Close:
                CommitLocked(device, {});
                result = Result::Closed;
                break;
            case ConsoleLineEditor::Action::None:
                break;
        }
    });
    if (result == Result::Closed)
        Detach();
    return result;
}

void ConsolePrompt::EraseLocked(ConsoleDevice& device)
{
    if (!_visible)
        return;
    if (device.SupportsVirtualTerminal())
        device.Write("\r\x1b[K");
    else
    {
        device.Write("\r");
        device.Write(std::string(_drawn, ' '));
        device.Write("\r");
    }
    _drawn = 0;
    _visible = false;
}

void ConsolePrompt::DrawLocked(ConsoleDevice& device, Draw draw)
{
    if (_visible || !_attached)
        return;
    std::string const& line = _editor.GetLine();
    std::size_t const prompt = ConsoleLineEditor::Columns(_text);
    std::string_view visible(line);
    std::size_t columns = ConsoleLineEditor::Columns(line);
    std::size_t after = 0;
    if (draw == Draw::Cursor)
    {
        after = _editor.GetColumnsAfterCursor();
        std::size_t const width = device.GetColumns();
        if (width != 0)
        {
            std::size_t const room = width > prompt + 1 ? width - prompt - 1 : 0;
            ConsoleLineEditor::Window const window = ConsoleLineEditor::Fit(line, _editor.GetCursor(), room);
            visible = std::string_view(line).substr(window.Start, window.End - window.Start);
            columns = window.Columns;
            after = window.Columns - window.CursorColumn;
        }
    }
    bool const color = _console.UsesColor();
    bool const ansi = device.SupportsVirtualTerminal();
    std::string output;
    if (color && ansi)
    {
        output.append(ConsoleWriter::GetAnsiSequence(PromptColor));
        output.append(_text);
        output.append("\x1b[0m");
    }
    else if (color)
    {
        device.SetLegacyColor(PromptColor);
        device.Write(_text);
        device.ResetLegacyColor();
    }
    else
        output.append(_text);
    output.append(visible);
    output.append(after, '\b');
    device.Write(output);
    _drawn = prompt + columns;
    _visible = true;
}

void ConsolePrompt::CommitLocked(ConsoleDevice& device, std::string_view suffix)
{
    EraseLocked(device);
    DrawLocked(device, Draw::Whole);
    std::string output(suffix);
    output.push_back('\n');
    device.Write(output);
    _drawn = 0;
    _visible = false;
    device.Flush();
}
