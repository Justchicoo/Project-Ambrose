/*
 * Project Ambrose by Imjustchico
 * Draws the command prompt and the line being typed on the shared console writer, kept to one row of the terminal window, erasing and redrawing itself around every log line so the two never mix.
 */

#ifndef AMBROSE_CONSOLEPROMPT_H
#define AMBROSE_CONSOLEPROMPT_H

#include "ConsoleLineEditor.h"
#include "ConsoleWriter.h"

#include <string>

class ConsolePrompt
{
public:
    static constexpr ConsoleColor PromptColor = ConsoleColor::Brown;

    enum class Result : uint8
    {
        Pending,
        Line,
        Closed
    };

    ConsolePrompt(ConsoleWriter& console, std::string text);
    ~ConsolePrompt();

    ConsolePrompt(ConsolePrompt const&) = delete;
    ConsolePrompt& operator=(ConsolePrompt const&) = delete;

    ConsoleLineEditor& Editor() noexcept;

    void Attach();
    void Detach();
    Result Apply(ConsoleKey const& key, std::string& line);

private:
    enum class Draw : uint8
    {
        Cursor,
        Whole
    };

    void EraseLocked(ConsoleDevice& device);
    void DrawLocked(ConsoleDevice& device, Draw draw);
    void CommitLocked(ConsoleDevice& device, std::string_view suffix);

    ConsoleWriter& _console;
    std::string _text;
    ConsoleLineEditor _editor;
    std::size_t _drawn = 0;
    bool _attached = false;
    bool _visible = false;
};

#endif
