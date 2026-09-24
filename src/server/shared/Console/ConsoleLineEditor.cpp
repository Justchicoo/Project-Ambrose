/*
 * Project Ambrose by Imjustchico
 * Applies one key at a time to the typed line: inserting and deleting whole characters and words, moving the cursor, walking the history around the line being written, completing a command name to its longest shared spelling, and fitting the line into the columns a terminal has room for.
 */

#include "ConsoleLineEditor.h"
#include "ConsoleTextWidth.h"
#include "StringUtil.h"

#include <algorithm>

namespace
{
    bool IsContinuation(char c)
    {
        return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
    }

    bool IsSeparator(char c)
    {
        return c == ' ' || c == '\t';
    }

    bool SameIgnoringCase(char left, char right)
    {
        return Ambrose::EqualsIgnoreCase(std::string_view(&left, 1), std::string_view(&right, 1));
    }
}

void ConsoleLineEditor::SetCompleter(Completer completer)
{
    _completer = std::move(completer);
}

ConsoleLineEditor::Action ConsoleLineEditor::Apply(ConsoleKey const& key)
{
    switch (key.Kind)
    {
        case ConsoleKeyKind::Character:
            if (key.Text.empty() || _line.size() + key.Text.size() > MaxLine)
                return Action::None;
            _line.insert(_cursor, key.Text);
            _cursor += key.Text.size();
            return Action::Redraw;
        case ConsoleKeyKind::Enter:
            return Action::Submit;
        case ConsoleKeyKind::Backspace:
        {
            if (_cursor == 0)
                return Action::None;
            std::size_t const previous = PreviousIndex(_cursor);
            _line.erase(previous, _cursor - previous);
            _cursor = previous;
            return Action::Redraw;
        }
        case ConsoleKeyKind::Delete:
            if (_cursor == _line.size())
                return Action::None;
            _line.erase(_cursor, NextIndex(_cursor) - _cursor);
            return Action::Redraw;
        case ConsoleKeyKind::Left:
            return MoveTo(PreviousIndex(_cursor));
        case ConsoleKeyKind::Right:
            return MoveTo(NextIndex(_cursor));
        case ConsoleKeyKind::WordLeft:
            return MoveTo(WordStart(_cursor));
        case ConsoleKeyKind::WordRight:
            return MoveTo(WordEnd(_cursor));
        case ConsoleKeyKind::Home:
            return MoveTo(0);
        case ConsoleKeyKind::End:
            return MoveTo(_line.size());
        case ConsoleKeyKind::Up:
            return Recall(true);
        case ConsoleKeyKind::Down:
            return Recall(false);
        case ConsoleKeyKind::Tab:
            return Complete();
        case ConsoleKeyKind::DeleteWord:
        {
            std::size_t const start = WordStart(_cursor);
            if (start == _cursor)
                return Action::None;
            _line.erase(start, _cursor - start);
            _cursor = start;
            return Action::Redraw;
        }
        case ConsoleKeyKind::ClearLine:
            if (_line.empty())
                return Action::None;
            _line.clear();
            _cursor = 0;
            return Action::Redraw;
        case ConsoleKeyKind::KillToEnd:
            if (_cursor == _line.size())
                return Action::None;
            _line.erase(_cursor);
            return Action::Redraw;
        case ConsoleKeyKind::Interrupt:
            return Action::Interrupt;
        case ConsoleKeyKind::EndOfFile:
            if (_line.empty())
                return Action::Close;
            if (_cursor == _line.size())
                return Action::None;
            _line.erase(_cursor, NextIndex(_cursor) - _cursor);
            return Action::Redraw;
        case ConsoleKeyKind::None:
            break;
    }
    return Action::None;
}

std::string ConsoleLineEditor::TakeLine()
{
    std::string line = std::move(_line);
    _line.clear();
    _cursor = 0;
    _draft.clear();
    _suggestions.clear();
    if (!Ambrose::Trim(line).empty() && (_history.empty() || _history.back() != line))
    {
        _history.push_back(line);
        if (_history.size() > MaxHistory)
            _history.erase(_history.begin());
    }
    _recall = _history.size();
    return line;
}

void ConsoleLineEditor::Clear()
{
    _line.clear();
    _cursor = 0;
    _draft.clear();
    _suggestions.clear();
    _recall = _history.size();
}

std::string const& ConsoleLineEditor::GetLine() const noexcept
{
    return _line;
}

std::size_t ConsoleLineEditor::GetCursor() const noexcept
{
    return _cursor;
}

std::size_t ConsoleLineEditor::GetColumnsAfterCursor() const
{
    return Columns(std::string_view(_line).substr(_cursor));
}

std::vector<std::string> const& ConsoleLineEditor::GetSuggestions() const noexcept
{
    return _suggestions;
}

std::vector<std::string> const& ConsoleLineEditor::GetHistory() const noexcept
{
    return _history;
}

std::size_t ConsoleLineEditor::Columns(std::string_view text)
{
    return ConsoleTextWidth::Columns(text);
}

ConsoleLineEditor::Window ConsoleLineEditor::Fit(std::string_view text, std::size_t cursor, std::size_t columns)
{
    if (cursor > text.size())
        cursor = text.size();
    Window window;
    window.CursorColumn = ConsoleTextWidth::Columns(text.substr(0, cursor));
    while (window.CursorColumn > columns && window.Start < cursor)
    {
        std::size_t const next = ConsoleTextWidth::Next(text, window.Start);
        window.CursorColumn -= ConsoleTextWidth::CharacterColumns(text.substr(window.Start, next - window.Start));
        window.Start = next;
    }
    window.End = cursor;
    window.Columns = window.CursorColumn;
    while (window.End < text.size())
    {
        std::size_t const next = ConsoleTextWidth::Next(text, window.End);
        std::size_t const step = ConsoleTextWidth::CharacterColumns(text.substr(window.End, next - window.End));
        if (window.Columns + step > columns)
            break;
        window.Columns += step;
        window.End = next;
    }
    return window;
}

ConsoleLineEditor::Action ConsoleLineEditor::Complete()
{
    _suggestions.clear();
    if (!_completer)
        return Action::None;
    if (_cursor < _line.size() && !IsSeparator(_line[_cursor]))
        return Action::None;
    std::string_view const prefix = std::string_view(_line).substr(0, _cursor);
    std::vector<std::string> candidates = _completer(prefix);
    if (candidates.empty())
        return Action::None;
    std::string insert = candidates.front();
    if (candidates.size() > 1)
    {
        for (std::string const& candidate : candidates)
        {
            std::size_t shared = 0;
            while (shared < insert.size() && shared < candidate.size() && SameIgnoringCase(insert[shared], candidate[shared]))
                ++shared;
            insert.resize(shared);
        }
        if (insert.size() <= prefix.size())
        {
            _suggestions = std::move(candidates);
            return Action::Suggest;
        }
    }
    else if (_cursor == _line.size())
        insert += ' ';
    if (_line.size() - _cursor + insert.size() > MaxLine)
        return Action::None;
    _line.replace(0, _cursor, insert);
    _cursor = insert.size();
    return Action::Redraw;
}

ConsoleLineEditor::Action ConsoleLineEditor::Recall(bool older)
{
    if (older)
    {
        if (_recall == 0)
            return Action::None;
        if (_recall == _history.size())
            _draft = _line;
        --_recall;
        _line = _history[_recall];
    }
    else
    {
        if (_recall >= _history.size())
            return Action::None;
        ++_recall;
        _line = _recall == _history.size() ? _draft : _history[_recall];
    }
    _cursor = _line.size();
    return Action::Redraw;
}

ConsoleLineEditor::Action ConsoleLineEditor::MoveTo(std::size_t cursor)
{
    if (cursor == _cursor)
        return Action::None;
    _cursor = cursor;
    return Action::Redraw;
}

std::size_t ConsoleLineEditor::PreviousIndex(std::size_t index) const
{
    while (index > 0)
    {
        --index;
        if (!IsContinuation(_line[index]))
            break;
    }
    return index;
}

std::size_t ConsoleLineEditor::NextIndex(std::size_t index) const
{
    if (index >= _line.size())
        return _line.size();
    ++index;
    while (index < _line.size() && IsContinuation(_line[index]))
        ++index;
    return index;
}

std::size_t ConsoleLineEditor::WordStart(std::size_t index) const
{
    while (index > 0 && IsSeparator(_line[index - 1]))
        --index;
    while (index > 0 && !IsSeparator(_line[index - 1]))
        --index;
    return index;
}

std::size_t ConsoleLineEditor::WordEnd(std::size_t index) const
{
    while (index < _line.size() && IsSeparator(_line[index]))
        ++index;
    while (index < _line.size() && !IsSeparator(_line[index]))
        ++index;
    return index;
}
