/*
 * Project Ambrose by Imjustchico
 * A console line source for an interactive terminal: it reads single key presses instead of whole lines, edits them at a drawn prompt, hands each finished line to the reader thread, and takes the prompt back and the terminal out of raw mode the moment input closes.
 */

#ifndef AMBROSE_TERMINALCONSOLEINPUT_H
#define AMBROSE_TERMINALCONSOLEINPUT_H

#include "ConsoleInput.h"
#include "ConsoleLineEditor.h"
#include "ConsolePrompt.h"

#include <deque>
#include <memory>
#include <vector>

class TerminalConsoleInput : public ConsoleInput
{
public:
    static constexpr std::size_t MaxPendingLines = 256;

    TerminalConsoleInput(ConsoleWriter& console, std::string prompt, ConsoleLineEditor::Completer completer);
    ~TerminalConsoleInput() override;

    static bool IsAvailable(ConsoleWriter& console);

    ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) override;
    void Interrupt() override;

private:
    struct State;

    bool ReadKeys(std::vector<ConsoleKey>& keys, std::chrono::milliseconds timeout);
    void RestoreMode();
    void Close();

    ConsolePrompt _prompt;
    ConsoleKeyDecoder _decoder;
    std::unique_ptr<State> _state;
    std::deque<std::string> _lines;
    std::atomic<bool> _interrupted{ false };
    std::atomic<bool> _closed{ false };
};

#endif
