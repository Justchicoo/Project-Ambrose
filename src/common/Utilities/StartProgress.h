/*
 * Project Ambrose by Imjustchico
 * What a starting process is doing and how long it may still take: a long start step names its stage and the time it may need, from wherever it runs, so whoever watches the start, such as the supervisor through the admin API's health answer or, with that API off, through the line a listener prints for each step, "<app> start step, up to <seconds> s: <stage>", waits for that step rather than ending a start that is still working, and gives up only when the step outruns what it asked for; cleared once the process is ready.
 */

#ifndef AMBROSE_STARTPROGRESS_H
#define AMBROSE_STARTPROGRESS_H

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

struct StartStep
{
    std::string Stage;
    std::chrono::system_clock::time_point Until;
};

namespace StartProgress
{
    using Listener = std::function<void(std::string_view stage, std::chrono::seconds allowance)>;

    void Report(std::string_view stage, std::chrono::seconds allowance);
    void SetListener(Listener listener);
    std::string StepText(std::string_view app, std::string_view stage, std::chrono::seconds allowance);
    std::string StepPrefix(std::string_view app);
    std::optional<StartStep> Current();
    void Clear();
}

#endif
