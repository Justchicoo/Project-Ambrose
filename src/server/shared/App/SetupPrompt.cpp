/*
 * Project Ambrose by Imjustchico
 * Prints each question with its numbered options, reads one answer line at a time with a watchdog that interrupts the input when the timeout passes, treats Enter as the first option or yes, s, skip, n or no as skipping, a number in range as that option and any other text as a path, asks again after an out-of-range number or an unclear yes or no up to three times, and stops asking once input closes or times out.
 */

#include "SetupPrompt.h"
#include "Environment.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ostream>
#include <thread>

SetupPrompt::SetupPrompt(std::unique_ptr<ConsoleInput> input, std::ostream& out, bool interactive, std::chrono::seconds timeout)
    : _input(std::move(input)), _out(out), _interactive(interactive && _input != nullptr), _timeout(timeout)
{
}

std::unique_ptr<SetupPrompt> SetupPrompt::ForProcess(std::ostream& out, bool enabled, std::chrono::seconds timeout)
{
    bool const interactive = enabled && Ambrose::IsInteractiveTerminal();
    return std::make_unique<SetupPrompt>(interactive ? std::make_unique<StandardConsoleInput>() : nullptr, out, interactive, timeout);
}

void SetupPrompt::Say(std::string_view text)
{
    if (!IsInteractive())
        return;
    _out << text << '\n';
    _out.flush();
}

SetupPrompt::Choice SetupPrompt::Choose(std::string_view question, std::vector<std::string> const& options)
{
    Choice choice;
    if (!IsInteractive())
        return choice;
    _out << question << '\n';
    for (std::size_t index = 0; index < options.size(); ++index)
        _out << fmt::format("  {}) {}\n", index + 1, options[index]);
    for (int attempt = 0; attempt < MaxAttempts; ++attempt)
    {
        if (options.empty())
            _out << "Type a path, or press Enter to skip: ";
        else if (options.size() == 1)
            _out << "Press Enter to use 1, type another path, or type s to skip: ";
        else
            _out << fmt::format("Choose 1-{}, press Enter for 1, type another path, or type s to skip: ", options.size());
        _out.flush();
        std::optional<std::string> const answer = ReadAnswer();
        if (!answer)
            return choice;
        std::string const text(Ambrose::Trim(*answer));
        std::string const lower = Ambrose::ToLower(text);
        if (lower == "s" || lower == "skip" || lower == "n" || lower == "no" || (text.empty() && options.empty()))
            return choice;
        if (text.empty())
        {
            choice.Kind = Answer::Picked;
            return choice;
        }
        if (std::optional<std::size_t> const number = Ambrose::StringTo<std::size_t>(text))
        {
            if (*number >= 1 && *number <= options.size())
            {
                choice.Kind = Answer::Picked;
                choice.Index = *number - 1;
                return choice;
            }
            _out << (options.empty() ? std::string("There is nothing to choose by number; type a path instead.\n") : fmt::format("Choose a number from 1 to {}.\n", options.size()));
            continue;
        }
        choice.Kind = Answer::Path;
        choice.Path = text;
        return choice;
    }
    return choice;
}

bool SetupPrompt::Confirm(std::string_view question)
{
    if (!IsInteractive())
        return false;
    for (int attempt = 0; attempt < MaxAttempts; ++attempt)
    {
        _out << question << " [Y/n]: ";
        _out.flush();
        std::optional<std::string> const answer = ReadAnswer();
        if (!answer)
            return false;
        std::string const lower = Ambrose::ToLower(Ambrose::Trim(*answer));
        if (lower.empty() || lower == "y" || lower == "yes")
            return true;
        if (lower == "n" || lower == "no")
            return false;
        _out << "Answer y or n.\n";
    }
    return false;
}

std::optional<std::string> SetupPrompt::ReadAnswer()
{
    std::mutex mutex;
    std::condition_variable wake;
    bool answered = false;
    std::atomic<bool> timedOut{ false };
    std::thread watchdog;
    if (_timeout.count() > 0)
    {
        watchdog = std::thread([this, &mutex, &wake, &answered, &timedOut]
        {
            std::unique_lock lock(mutex);
            if (!wake.wait_for(lock, _timeout, [&answered] { return answered; }))
            {
                timedOut = true;
                _input->Interrupt();
            }
        });
    }
    std::optional<std::string> result;
    std::string line;
    while (true)
    {
        ConsoleInput::ReadResult const read = _input->ReadLine(line, PollInterval);
        if (read == ConsoleInput::ReadResult::Line)
        {
            result = line;
            break;
        }
        if (read == ConsoleInput::ReadResult::Closed || timedOut)
            break;
    }
    {
        std::lock_guard const lock(mutex);
        answered = true;
    }
    wake.notify_all();
    if (watchdog.joinable())
        watchdog.join();
    if (!result)
    {
        _closed = true;
        _out << (timedOut ? "\nNo answer in time; skipping setup questions.\n" : "\nInput closed; skipping setup questions.\n");
        _out.flush();
    }
    return result;
}
