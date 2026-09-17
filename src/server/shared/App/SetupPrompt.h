/*
 * Project Ambrose by Imjustchico
 * Questions a server or tool asks the person running it when setup runs in ask mode: a numbered choice that also takes a typed path or a skip, and a yes-or-no confirmation; they are asked only on an interactive terminal, every answer waits at most a timeout, a timeout, closed input or a stop request skips the question and all later ones, and the prompt tells why it cannot ask.
 */

#ifndef AMBROSE_SETUPPROMPT_H
#define AMBROSE_SETUPPROMPT_H

#include "ConsoleInput.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class SetupPrompt
{
public:
    enum class Answer
    {
        Picked,
        Path,
        Skipped
    };

    enum class Status
    {
        Interactive,
        Disabled,
        NotATerminal,
        InputClosed,
        TimedOut,
        Stopped
    };

    struct Choice
    {
        Answer Kind = Answer::Skipped;
        std::size_t Index = 0;
        std::string Path;
    };

    static constexpr int MaxAttempts = 3;
    static constexpr std::chrono::milliseconds PollInterval{ 200 };

    SetupPrompt(std::unique_ptr<ConsoleInput> input, std::ostream& out, bool interactive, std::chrono::seconds timeout);
    SetupPrompt(SetupPrompt const&) = delete;
    SetupPrompt& operator=(SetupPrompt const&) = delete;

    static std::unique_ptr<SetupPrompt> ForProcess(std::ostream& out, bool enabled, std::chrono::seconds timeout);

    void SetCancellation(std::function<bool()> cancelled);

    bool IsInteractive() const noexcept { return _interactive && !_closed; }
    Status GetStatus() const noexcept;
    void Say(std::string_view text);
    Choice Choose(std::string_view question, std::vector<std::string> const& options);
    bool Confirm(std::string_view question);

private:
    bool StopBeforeAsking();
    void Close(Status status);
    std::optional<std::string> ReadAnswer();

    std::unique_ptr<ConsoleInput> _input;
    std::ostream& _out;
    bool _interactive;
    std::chrono::seconds _timeout;
    std::function<bool()> _cancelled;
    bool _closed = false;
    Status _status;
};

#endif
