/*
 * Project Ambrose by Imjustchico
 * Holds the one start step a process last reported behind a lock, so any thread may report while the admin API reads it, and tells the listener of each report outside that lock.
 */

#include "StartProgress.h"

#include <fmt/format.h>

#include <mutex>
#include <utility>

namespace
{
    std::mutex& Guard()
    {
        static std::mutex guard;
        return guard;
    }

    std::optional<StartStep>& Held()
    {
        static std::optional<StartStep> held;
        return held;
    }

    StartProgress::Listener& HeldListener()
    {
        static StartProgress::Listener listener;
        return listener;
    }
}

void StartProgress::Report(std::string_view stage, std::chrono::seconds allowance)
{
    Listener listener;
    {
        std::lock_guard const lock(Guard());
        Held() = StartStep{ std::string(stage), std::chrono::system_clock::now() + allowance };
        listener = HeldListener();
    }
    if (listener)
        listener(stage, allowance);
}

void StartProgress::SetListener(Listener listener)
{
    std::lock_guard const lock(Guard());
    HeldListener() = std::move(listener);
}

std::optional<StartStep> StartProgress::Current()
{
    std::lock_guard const lock(Guard());
    return Held();
}

void StartProgress::Clear()
{
    std::lock_guard const lock(Guard());
    Held().reset();
}

std::string StartProgress::StepText(std::string_view app, std::string_view stage, std::chrono::seconds allowance)
{
    return fmt::format("{}{} s: {}", StepPrefix(app), allowance.count(), stage);
}

std::string StartProgress::StepPrefix(std::string_view app)
{
    return fmt::format("{} start step, up to ", app);
}
