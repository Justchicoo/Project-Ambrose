/*
 * Project Ambrose by Imjustchico
 * The state of one console input line: the text, the cursor, recalled history, command-name completion and the display width of what is typed, driven by keys and independent of any terminal.
 */

#ifndef AMBROSE_CONSOLELINEEDITOR_H
#define AMBROSE_CONSOLELINEEDITOR_H

#include "ConsoleKeyDecoder.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class ConsoleLineEditor
{
public:
    using Completer = std::function<std::vector<std::string>(std::string_view prefix)>;

    enum class Action : uint8
    {
        None,
        Redraw,
        Suggest,
        Submit,
        Interrupt,
        Close
    };

    struct Window
    {
        std::size_t Start = 0;
        std::size_t End = 0;
        std::size_t CursorColumn = 0;
        std::size_t Columns = 0;
    };

    static constexpr std::size_t MaxLine = 4096;
    static constexpr std::size_t MaxHistory = 200;

    void SetCompleter(Completer completer);
    Action Apply(ConsoleKey const& key);
    std::string TakeLine();
    void Clear();

    std::string const& GetLine() const noexcept;
    std::size_t GetCursor() const noexcept;
    std::size_t GetColumnsAfterCursor() const;
    std::vector<std::string> const& GetSuggestions() const noexcept;
    std::vector<std::string> const& GetHistory() const noexcept;

    static std::size_t Columns(std::string_view text);
    static Window Fit(std::string_view text, std::size_t cursor, std::size_t columns);

private:
    Action Complete();
    Action Recall(bool older);
    Action MoveTo(std::size_t cursor);
    std::size_t PreviousIndex(std::size_t index) const;
    std::size_t NextIndex(std::size_t index) const;
    std::size_t WordStart(std::size_t index) const;
    std::size_t WordEnd(std::size_t index) const;

    Completer _completer;
    std::string _line;
    std::string _draft;
    std::vector<std::string> _history;
    std::vector<std::string> _suggestions;
    std::size_t _cursor = 0;
    std::size_t _recall = 0;
};

#endif
