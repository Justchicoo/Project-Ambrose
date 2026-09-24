/*
 * Project Ambrose by Imjustchico
 * Runs the targets and remembers what happened: the order is worked out from what each target says it follows, a target whose dependencies cannot be satisfied is still run last rather than dropped, because a cycle in the register is the operator's problem to see and not a reason to stop reloading anything, and a loader that throws is treated as a loader that failed, since a store that was serving before must go on serving.
 */

#include "ReloadMgr.h"
#include "Log.h"
#include "MetricRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <unordered_set>
#include <utility>

ReloadMgr& ReloadMgr::Instance()
{
    static ReloadMgr instance;
    return instance;
}

bool ReloadMgr::Register(std::string name, Loader loader, std::vector<std::string> dependsOn)
{
    if (name.empty() || !loader)
        return false;
    std::lock_guard const lock(_mutex);
    Target target;
    target.Name = name;
    target.Load = std::move(loader);
    target.DependsOn = std::move(dependsOn);
    target.Last.Target = name;
    Ambrose::MetricLabels const label{ { "target", name } };
    target.Reloads = &sMetrics.CounterFor("ambrose_reloads_total", "Reloads asked of a target", label);
    target.Failures = &sMetrics.CounterFor("ambrose_reload_failures_total", "Reloads that left the old contents serving", label);
    target.Seconds = &sMetrics.HistogramFor("ambrose_reload_seconds", "How long rebuilding a target took", label);
    auto const [it, inserted] = _targets.insert_or_assign(std::move(name), std::move(target));
    (void)it;
    return inserted;
}

bool ReloadMgr::Unregister(std::string_view name)
{
    std::lock_guard const lock(_mutex);
    auto const it = _targets.find(std::string(name));
    if (it == _targets.end())
        return false;
    _targets.erase(it);
    return true;
}

bool ReloadMgr::IsRegistered(std::string_view name) const
{
    std::lock_guard const lock(_mutex);
    return _targets.contains(std::string(name));
}

void ReloadMgr::Clear()
{
    std::lock_guard const lock(_mutex);
    _targets.clear();
}

std::vector<std::string> ReloadMgr::GetTargets() const
{
    std::lock_guard const lock(_mutex);
    std::vector<std::string> names;
    names.reserve(_targets.size());
    for (auto const& [name, target] : _targets)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ReloadMgr::GetOrderedTargets() const
{
    std::lock_guard const lock(_mutex);
    return OrderLocked();
}

std::optional<ReloadOutcome> ReloadMgr::GetLastOutcome(std::string_view name) const
{
    std::lock_guard const lock(_mutex);
    auto const it = _targets.find(std::string(name));
    if (it == _targets.end() || !it->second.Ran)
        return std::nullopt;
    return it->second.Last;
}

uint64 ReloadMgr::GetGeneration(std::string_view name) const
{
    std::lock_guard const lock(_mutex);
    auto const it = _targets.find(std::string(name));
    return it == _targets.end() ? 0 : it->second.Generation;
}

std::vector<std::string> ReloadMgr::OrderLocked() const
{
    std::vector<std::string> names;
    names.reserve(_targets.size());
    for (auto const& [name, target] : _targets)
        names.push_back(name);
    std::sort(names.begin(), names.end());

    std::vector<std::string> ordered;
    ordered.reserve(names.size());
    std::unordered_set<std::string> placed;
    bool progressed = true;
    while (progressed && ordered.size() < names.size())
    {
        progressed = false;
        for (std::string const& name : names)
        {
            if (placed.contains(name))
                continue;
            auto const it = _targets.find(name);
            bool ready = true;
            for (std::string const& dependency : it->second.DependsOn)
                if (_targets.contains(dependency) && !placed.contains(dependency))
                {
                    ready = false;
                    break;
                }
            if (!ready)
                continue;
            ordered.push_back(name);
            placed.insert(name);
            progressed = true;
        }
    }
    for (std::string const& name : names)
        if (!placed.contains(name))
            ordered.push_back(name);
    return ordered;
}

ReloadOutcome ReloadMgr::RunLocked(Target& target)
{
    ReloadOutcome outcome;
    outcome.Target = target.Name;
    outcome.Ok = false;
    auto const started = std::chrono::steady_clock::now();
    try
    {
        outcome.Ok = target.Load(outcome.Errors);
    }
    catch (std::exception const& exception)
    {
        outcome.Ok = false;
        outcome.Errors.emplace_back(exception.what());
    }
    catch (...)
    {
        outcome.Ok = false;
        outcome.Errors.emplace_back("the loader threw an exception that carried no message");
    }
    if (outcome.Ok)
        ++target.Generation;
    else if (outcome.Errors.empty())
        outcome.Errors.emplace_back("the loader refused the new contents but named no reason");
    outcome.Generation = target.Generation;
    target.Ran = true;
    target.Last = outcome;
    target.Seconds->Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    target.Reloads->Add();
    if (!outcome.Ok)
        target.Failures->Add();
    if (outcome.Ok)
        LOG_INFO("server.reload", "Reloaded {}, now generation {}", target.Name, target.Generation);
    else
        LOG_ERROR("server.reload", "{} was not reloaded and generation {} goes on serving: {}",
            target.Name, target.Generation, outcome.Errors.front());
    return outcome;
}

ReloadOutcome ReloadMgr::Reload(std::string_view name)
{
    std::lock_guard const lock(_mutex);
    auto const it = _targets.find(std::string(name));
    if (it == _targets.end())
    {
        ReloadOutcome missing;
        missing.Target = std::string(name);
        missing.Ok = false;
        missing.Errors.emplace_back("there is nothing registered by that name");
        return missing;
    }
    return RunLocked(it->second);
}

std::vector<ReloadOutcome> ReloadMgr::ReloadAll()
{
    std::lock_guard const lock(_mutex);
    std::vector<ReloadOutcome> outcomes;
    for (std::string const& name : OrderLocked())
    {
        auto const it = _targets.find(name);
        if (it == _targets.end())
            continue;
        outcomes.push_back(RunLocked(it->second));
    }
    return outcomes;
}

std::vector<std::string> ReloadMgr::Describe(ReloadOutcome const& outcome)
{
    std::vector<std::string> lines;
    if (outcome.Ok)
    {
        lines.push_back(fmt::format("{} is now generation {}", outcome.Target, outcome.Generation));
        return lines;
    }
    lines.push_back(fmt::format("{} was not reloaded and generation {} goes on serving", outcome.Target, outcome.Generation));
    for (std::string const& error : outcome.Errors)
        lines.push_back(fmt::format("  {}", error));
    return lines;
}

std::vector<std::string> ReloadMgr::DescribeTargets() const
{
    std::lock_guard const lock(_mutex);
    std::vector<std::string> const ordered = OrderLocked();
    std::vector<std::string> lines;
    if (ordered.empty())
    {
        lines.emplace_back("This app has nothing registered that can be reloaded on its own");
        return lines;
    }
    lines.push_back(fmt::format("{} target(s), in the order they are reloaded:", ordered.size()));
    for (std::string const& name : ordered)
    {
        auto const it = _targets.find(name);
        if (it == _targets.end())
            continue;
        if (!it->second.Ran)
            lines.push_back(fmt::format("  {} at generation {}, not reloaded since this app started", name, it->second.Generation));
        else
            lines.push_back(fmt::format("  {} at generation {}, last attempt {}", name, it->second.Generation, it->second.Last.Ok ? "held" : "kept the one before"));
    }
    return lines;
}
