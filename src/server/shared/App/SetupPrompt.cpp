/*
 * Project Ambrose by Imjustchico
 * Prints each question with its numbered options after flushing the log, switching a Windows console to UTF-8 first, reads one answer line at a time with a watchdog that polls the stop request every PollInterval, even without a timeout, and interrupts the input on a stop or when the timeout passes, discards a line that arrives as the watchdog fires so a timeout is always reported as one, treats Enter as the first option or yes, s, skip, n or no as skipping, a number in range as that option and any other text as a path, asks again after an out-of-range number or an unclear yes or no up to three times, and stops asking once input closes, times out or a stop is requested.
 */

#include "SetupPrompt.h"
#include "Environment.h"
#include "Log.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ostream>
#include <thread>

SetupPrompt::SetupPrompt(std::unique_ptr<ConsoleInput> input, std::ostream& out, bool interactive, std::chrono::seconds timeout)
    : _input(std::move(input)), _out(out), _interactive(interactive && _input != nullptr), _timeout(timeout), _status(_interactive ? Status::Interactive : Status::NotATerminal)
{
}

std::unique_ptr<SetupPrompt> SetupPrompt::ForProcess(std::ostream& out, bool enabled, std::chrono::seconds timeout)
{
    bool const interactive = enabled && Ambrose::IsInteractiveTerminal();
    if (interactive)
        Ambrose::UseUtf8Console();
    std::unique_ptr<SetupPrompt> prompt = std::make_unique<SetupPrompt>(interactive ? std::make_unique<StandardConsoleInput>() : nullptr, out, interactive, timeout);
    if (!enabled)
        prompt->_status = Status::Disabled;
    return prompt;
}

void SetupPrompt::SetCancellation(std::function<bool()> cancelled)
{
    _cancelled = std::move(cancelled);
}

SetupPrompt::Status SetupPrompt::GetStatus() const noexcept
{
    return _status;
}

void SetupPrompt::Say(std::string_view text)
{
    if (!IsInteractive())
        return;
    sLog.Flush();
    _out << text << '\n';
    _out.flush();
}

SetupPrompt::Choice SetupPrompt::Choose(std::string_view question, std::vector<std::string> const& options)
{
    Choice choice;
    if (!IsInteractive() || StopBeforeAsking())
        return choice;
    sLog.Flush();
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
    if (!IsInteractive() || StopBeforeAsking())
        return false;
    sLog.Flush();
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

bool SetupPrompt::StopBeforeAsking()
{
    if (!_cancelled || !_cancelled())
        return false;
    Close(Status::Stopped);
    return true;
}

void SetupPrompt::Close(Status status)
{
    _closed = true;
    _status = status;
    if (status == Status::TimedOut)
        _out << "\nNo answer in time; skipping setup questions.\n";
    else if (status == Status::Stopped)
        _out << "\nStop requested; skipping setup questions.\n";
    else
        _out << "\nInput closed; skipping setup questions.\n";
    _out.flush();
}

std::optional<std::string> SetupPrompt::ReadAnswer()
{
    std::mutex mutex;
    std::condition_variable wake;
    bool answered = false;
    std::atomic<bool> timedOut{ false };
    std::atomic<bool> stopped{ false };
    std::thread watchdog;
    if (_timeout.count() > 0 || _cancelled)
    {
        watchdog = std::thread([this, &mutex, &wake, &answered, &timedOut, &stopped]
        {
            auto const deadline = std::chrono::steady_clock::now() + _timeout;
            std::unique_lock lock(mutex);
            while (!answered)
            {
                std::chrono::steady_clock::duration wait = _cancelled ? std::chrono::steady_clock::duration(PollInterval) : deadline - std::chrono::steady_clock::now();
                if (_timeout.count() > 0)
                    wait = std::min(wait, deadline - std::chrono::steady_clock::now());
                if (wait.count() > 0 && wake.wait_for(lock, wait, [&answered] { return answered; }))
                    break;
                if (_cancelled)
                {
                    lock.unlock();
                    bool const cancel = _cancelled();
                    lock.lock();
                    if (answered)
                        break;
                    if (cancel)
                    {
                        stopped = true;
                        _input->Interrupt();
                        break;
                    }
                }
                if (_timeout.count() > 0 && std::chrono::steady_clock::now() >= deadline)
                {
                    timedOut = true;
                    _input->Interrupt();
                    break;
                }
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
        if (read == ConsoleInput::ReadResult::Closed || timedOut || stopped)
            break;
    }
    {
        std::lock_guard const lock(mutex);
        answered = true;
    }
    wake.notify_all();
    if (watchdog.joinable())
        watchdog.join();
    if (timedOut || stopped)
        result.reset();
    if (!result)
        Close(stopped ? Status::Stopped : timedOut ? Status::TimedOut : Status::InputClosed);
    return result;
}
