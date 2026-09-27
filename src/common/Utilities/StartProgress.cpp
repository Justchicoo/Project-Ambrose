/*
 * Project Ambrose by Imjustchico
 * Holds the one start step a process last reported behind a lock, so any thread may report while the admin API reads it.
 */

#include "StartProgress.h"

#include <mutex>

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
}

void StartProgress::Report(std::string_view stage, std::chrono::seconds allowance)
{
    std::lock_guard const lock(Guard());
    Held() = StartStep{ std::string(stage), std::chrono::system_clock::now() + allowance };
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
