/*
 * Project Ambrose by Imjustchico
 * Questions a server or tool asks the person running it during guided setup: a numbered choice that also takes a typed path or a skip, and a yes-or-no confirmation; they are asked only on an interactive terminal, and every answer waits at most a timeout, after which the question and all later ones are skipped.
 */

#ifndef AMBROSE_SETUPPROMPT_H
#define AMBROSE_SETUPPROMPT_H

#include "ConsoleInput.h"

#include <chrono>
#include <cstddef>
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

    bool IsInteractive() const noexcept { return _interactive && !_closed; }
    void Say(std::string_view text);
    Choice Choose(std::string_view question, std::vector<std::string> const& options);
    bool Confirm(std::string_view question);

private:
    std::optional<std::string> ReadAnswer();

    std::unique_ptr<ConsoleInput> _input;
    std::ostream& _out;
    bool _interactive;
    std::chrono::seconds _timeout;
    bool _closed = false;
};

#endif
