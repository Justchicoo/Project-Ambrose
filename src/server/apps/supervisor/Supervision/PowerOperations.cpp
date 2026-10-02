/*
 * Project Ambrose by Imjustchico
 * Runs each power operation as tiers: the loginserver, the patchserver and every app that is not a gameserver form the first tier and the gameservers the second, so a start or the start half of a restart runs the first tier and then the second and stops at the first tier that fails, while a stop, a kill or the stop half of a restart runs them the other way round and only the first tier it stops counts down, the players having been told by then. Every app of a tier is asked at once and watched until it settles: a start or restart is done when a new run reports ready and failed when that run ends before it, a stop or kill when the app is down, and any of them failed past its stop timeout, its start timeout and the longest start step an app may ask for. An app already where the action leaves it, or disabled while the stack starts, is skipped with the reason; a refusal from a single app is checked before anything is locked, so a request that cannot apply is answered at once rather than as an operation that fails. Locks are released before the result is reported, so whoever acts on the result finds them free.
 */

#include "PowerOperations.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>
#include <random>

namespace
{
    bool Alive(AppState state) noexcept
    {
        return state == AppState::Starting || state == AppState::Running || state == AppState::Stopping;
    }

    std::string DescribeTarget(PowerTarget const& target)
    {
        switch (target.Kind)
        {
            case PowerTargetKind::App: return target.Name;
            case PowerTargetKind::Realm: return fmt::format("the gameservers of realm {}", target.Name);
            case PowerTargetKind::Stack: return "the stack";
        }
        return target.Name;
    }

    bool Skippable(PowerAction action, std::string_view code) noexcept
    {
        return (action == PowerAction::Start && code == "already_running") || (action == PowerAction::Stop && (code == "already_stopped" || code == "already_stopping"))
            || (action == PowerAction::Kill && code == "not_running") || code == "disabled";
    }
}

OperationLease::OperationLease(PowerOperations& owner, ManagedApp& app, OperationHolder holder) : _owner(owner), _app(app), _holder(std::move(holder))
{
}

OperationLease::~OperationLease()
{
    _app.Hold(std::nullopt);
    _owner.Release(_holder.Id);
}

void OperationLease::Progress(std::string text)
{
    _app.HoldProgress(std::move(text));
}

PowerOperations::PowerOperations(std::vector<ManagedApp*> apps) : _apps(std::move(apps))
{
}

PowerOperations::~PowerOperations()
{
    Shutdown();
}

std::string_view PowerOperations::TargetKindName(PowerTargetKind kind) noexcept
{
    switch (kind)
    {
        case PowerTargetKind::App: return "app";
        case PowerTargetKind::Realm: return "realm";
        case PowerTargetKind::Stack: return "stack";
    }
    return "app";
}

std::optional<PowerTargetKind> PowerOperations::ParseTargetKind(std::string_view text) noexcept
{
    for (PowerTargetKind const kind : { PowerTargetKind::App, PowerTargetKind::Realm, PowerTargetKind::Stack })
        if (TargetKindName(kind) == text)
            return kind;
    return std::nullopt;
}

std::string PowerOperations::DescribeHolder(OperationHolder const& holder)
{
    return fmt::format("{} {} by {}, begun at {} ms", holder.Action, holder.Id, holder.By.empty() ? std::string("the supervisor") : holder.By, holder.StartedEpochMs);
}

int PowerOperations::Tier(AppSnapshot const& snapshot) noexcept
{
    return snapshot.ProgramName == "gameserver" ? 1 : 0;
}

int64 PowerOperations::NowEpochMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string PowerOperations::NewId()
{
    static thread_local std::mt19937_64 generator{ std::random_device{}() };
    return fmt::format("op-{:016x}", generator());
}

void PowerOperations::SetObservers(PowerObservers observers)
{
    std::lock_guard<std::mutex> const lock(_observerMutex);
    _observers = std::move(observers);
}

void PowerOperations::SetProtectedHours(std::optional<ProtectedHours> hours, std::string problem)
{
    std::lock_guard<std::mutex> const lock(_settingsMutex);
    _hours = std::move(hours);
    _hoursProblem = std::move(problem);
}

void PowerOperations::SetClock(std::function<std::chrono::system_clock::time_point()> clock)
{
    std::lock_guard<std::mutex> const lock(_settingsMutex);
    _clock = std::move(clock);
}

void PowerOperations::Shutdown()
{
    _stopping = true;
    std::vector<std::unique_ptr<Running>> running;
    {
        std::lock_guard<std::mutex> const lock(_runningMutex);
        running.swap(_running);
    }
    for (std::unique_ptr<Running>& entry : running)
        if (entry->Worker.joinable())
            entry->Worker.join();
}

void PowerOperations::Reap()
{
    std::vector<std::unique_ptr<Running>> finished;
    {
        std::lock_guard<std::mutex> const lock(_runningMutex);
        auto const done = std::stable_partition(_running.begin(), _running.end(), [](std::unique_ptr<Running> const& entry) { return !entry->Done.load(); });
        std::move(done, _running.end(), std::back_inserter(finished));
        _running.erase(done, _running.end());
    }
    for (std::unique_ptr<Running>& entry : finished)
        if (entry->Worker.joinable())
            entry->Worker.join();
}

std::optional<std::string> PowerOperations::ProtectedRefusal(std::string_view what, PowerOrigin origin, bool overridden, std::string& window) const
{
    std::lock_guard<std::mutex> const lock(_settingsMutex);
    bool const mayOverride = origin == PowerOrigin::Request && overridden;
    if (!_hoursProblem.empty())
    {
        window = "unreadable";
        if (mayOverride)
            return std::nullopt;
        return fmt::format("A {} is refused while the protected hours cannot be read: {}", what, _hoursProblem);
    }
    if (!_hours)
        return std::nullopt;
    std::optional<ProtectedWindow> const covering = _hours->Covering(_clock ? _clock() : std::chrono::system_clock::now());
    if (!covering)
        return std::nullopt;
    window = _hours->Describe(*covering);
    if (mayOverride)
        return std::nullopt;
    if (origin == PowerOrigin::Schedule)
        return fmt::format("A scheduled {} is refused during the protected hours {}", what, window);
    return fmt::format("A {} is refused during the protected hours {}; an owner may override it with a reason", what, window);
}

std::vector<ManagedApp*> PowerOperations::Resolve(PowerTarget const& target, PowerBegun& refusal) const
{
    std::vector<ManagedApp*> found;
    for (ManagedApp* const app : _apps)
    {
        AppSnapshot const snapshot = app->Snapshot();
        bool const named = target.Kind == PowerTargetKind::Stack || (target.Kind == PowerTargetKind::App && snapshot.Name == target.Name)
            || (target.Kind == PowerTargetKind::Realm && snapshot.ProgramName == "gameserver" && ManagedApp::RealmOf(snapshot) == target.Name);
        if (named)
            found.push_back(app);
    }
    if (found.empty())
    {
        refusal.Accepted = false;
        refusal.Status = 404;
        refusal.Code = target.Kind == PowerTargetKind::App ? "unknown_app" : target.Kind == PowerTargetKind::Realm ? "unknown_realm" : "empty_stack";
        refusal.Message = target.Kind == PowerTargetKind::App ? fmt::format("The supervisor runs no app named {}", target.Name)
            : target.Kind == PowerTargetKind::Realm ? fmt::format("The supervisor runs no gameserver for realm {}", target.Name) : std::string("The supervisor runs no apps");
    }
    return found;
}

std::vector<std::string> PowerOperations::Members(PowerTarget const& target, PowerBegun& refusal) const
{
    std::vector<std::string> names;
    for (ManagedApp* const app : Resolve(target, refusal))
        names.push_back(app->GetDefinition().Name);
    return names;
}

std::optional<OperationHolder> PowerOperations::Blocker(std::vector<ManagedApp*> const& apps, bool stack) const
{
    if (_stackLock)
        return _stackLock;
    if (stack && !_appLocks.empty())
        return _appLocks.begin()->second;
    for (ManagedApp* const app : apps)
        if (auto const held = _appLocks.find(app->GetDefinition().Name); held != _appLocks.end())
            return held->second;
    return std::nullopt;
}

std::optional<OperationHolder> PowerOperations::HolderOf(std::string_view app) const
{
    std::lock_guard<std::mutex> const lock(_lockMutex);
    if (auto const held = _appLocks.find(app); held != _appLocks.end())
        return held->second;
    return _stackLock;
}

void PowerOperations::Release(std::string const& id)
{
    std::lock_guard<std::mutex> const lock(_lockMutex);
    std::erase_if(_appLocks, [&id](auto const& entry) { return entry.second.Id == id; });
    if (_stackLock && _stackLock->Id == id)
        _stackLock.reset();
}

PowerBegun PowerOperations::Begin(PowerAsk ask)
{
    Reap();
    PowerBegun begun;
    if (_stopping)
        return { false, 503, "stopping", "The supervisor is stopping, so it starts no new operation" };
    std::vector<ManagedApp*> const apps = Resolve(ask.Target, begun);
    if (apps.empty())
        return begun;
    std::string window;
    if (ask.Action == PowerAction::Restart)
        if (std::optional<std::string> refused = ProtectedRefusal("restart", ask.Origin, ask.Override, window))
        {
            begun = { false, 409, "protected_hours", std::move(*refused) };
            begun.Window = window;
            return begun;
        }
    if (ask.Target.Kind == PowerTargetKind::App)
        if (PowerResult const refused = apps.front()->Check(ask.Action); !refused.Accepted)
        {
            begun = { false, refused.Status, refused.Code, refused.Message };
            begun.Holder = HolderOf(ask.Target.Name);
            return begun;
        }
    for (ManagedApp* const app : apps)
    {
        AppSnapshot const snapshot = app->Snapshot();
        if (snapshot.Held)
        {
            begun = { false, 409, "protected", fmt::format("{} is {} for {}, so {} cannot {}", snapshot.Name, ManagedApp::StateName(snapshot.Held->State), snapshot.Held->Holder,
                DescribeTarget(ask.Target), ManagedApp::ActionName(ask.Action)) };
            begun.Holder = HolderOf(snapshot.Name);
            return begun;
        }
    }

    bool const stack = ask.Target.Kind == PowerTargetKind::Stack;
    OperationHolder const holder{ NewId(), std::string(ManagedApp::ActionName(ask.Action)), ask.By, NowEpochMs() };
    {
        std::lock_guard<std::mutex> const lock(_lockMutex);
        if (ask.Action != PowerAction::Kill)
            if (std::optional<OperationHolder> const blocker = Blocker(apps, stack))
            {
                begun = { false, 409, "locked", fmt::format("{} cannot {} while it is held by {}", DescribeTarget(ask.Target), ManagedApp::ActionName(ask.Action), DescribeHolder(*blocker)) };
                begun.Holder = blocker;
                return begun;
            }
        for (ManagedApp* const app : apps)
            _appLocks.try_emplace(app->GetDefinition().Name, holder);
        if (stack && !_stackLock)
            _stackLock = holder;
    }

    PowerReport report;
    report.Operation = holder.Id;
    report.Ask = std::move(ask);
    report.OverriddenWindow = window;
    for (ManagedApp* const app : apps)
        report.Apps.push_back(app->GetDefinition().Name);
    begun.Accepted = true;
    begun.Status = 202;
    begun.Operation = holder.Id;
    begun.Apps = report.Apps;
    begun.Window = window;
    begun.Message = fmt::format("{} {}: accepted as {}", ManagedApp::ActionName(report.Ask.Action), DescribeTarget(report.Ask.Target), holder.Id);
    {
        std::lock_guard<std::mutex> const lock(_observerMutex);
        if (_observers.Accepted)
            _observers.Accepted(report);
    }
    auto entry = std::make_unique<Running>();
    Running* const running = entry.get();
    {
        std::lock_guard<std::mutex> const lock(_runningMutex);
        _running.push_back(std::move(entry));
        running->Worker = std::thread([this, running, report, apps]
        {
            Run(report, apps);
            running->Done = true;
        });
    }
    return begun;
}

std::shared_ptr<OperationLease> PowerOperations::Hold(std::string_view name, AppState state, std::string by, PowerBegun& refusal)
{
    if (!ManagedApp::IsProtected(state))
    {
        refusal = { false, 500, "not_protected", fmt::format("{} is not a protected state", ManagedApp::StateName(state)) };
        return nullptr;
    }
    std::vector<ManagedApp*> const apps = Resolve(PowerTarget{ PowerTargetKind::App, std::string(name) }, refusal);
    if (apps.empty())
        return nullptr;
    OperationHolder const holder{ NewId(), std::string(ManagedApp::StateName(state)), by, NowEpochMs() };
    {
        std::lock_guard<std::mutex> const lock(_lockMutex);
        if (std::optional<OperationHolder> const blocker = Blocker(apps, false))
        {
            refusal = { false, 409, "locked", fmt::format("{} cannot begin {} while it is held by {}", name, ManagedApp::StateName(state), DescribeHolder(*blocker)) };
            refusal.Holder = blocker;
            return nullptr;
        }
        _appLocks.emplace(std::string(name), holder);
    }
    apps.front()->Hold(AppHold{ state, by, holder.Id, holder.StartedEpochMs, {} });
    refusal = { true, 200, {}, {}, holder.Id };
    return std::make_shared<OperationLease>(*this, *apps.front(), holder);
}

void PowerOperations::Report(std::string const& operation, std::string const& app, std::string const& step, std::string const& outcome, std::string const& message)
{
    std::lock_guard<std::mutex> const lock(_observerMutex);
    if (_observers.Progress)
        _observers.Progress(PowerStep{ operation, app, step, outcome, message });
}

bool PowerOperations::RunTier(PowerReport const& report, std::vector<ManagedApp*> const& apps, PowerAction action, uint32 seconds, std::string& failure)
{
    struct Watch
    {
        ManagedApp* App = nullptr;
        AppSnapshot Base;
        std::chrono::steady_clock::time_point Deadline;
        bool Settled = false;
    };
    std::string const step(ManagedApp::ActionName(action));
    std::vector<Watch> watches;
    bool succeeded = true;
    for (ManagedApp* const app : apps)
    {
        Watch watch{ app, app->Snapshot() };
        AppDefinition const& definition = app->GetDefinition();
        std::chrono::seconds const stopBudget = std::chrono::seconds(seconds) + definition.StopTimeout + SettleMargin;
        std::chrono::seconds const startBudget = definition.StartTimeout + std::chrono::duration_cast<std::chrono::seconds>(ManagedApp::MaxStartGrant) + SettleMargin;
        watch.Deadline = std::chrono::steady_clock::now() + (action == PowerAction::Start ? startBudget : action == PowerAction::Restart ? stopBudget + startBudget : stopBudget);
        PowerResult const asked = app->Power(action, seconds);
        if (!asked.Accepted)
        {
            if (Skippable(action, asked.Code))
            {
                Report(report.Operation, definition.Name, step, "skipped", asked.Message);
                continue;
            }
            Report(report.Operation, definition.Name, step, "failed", asked.Message);
            failure = asked.Message;
            succeeded = false;
            continue;
        }
        Report(report.Operation, definition.Name, step, "begun", asked.Message);
        watches.push_back(std::move(watch));
    }
    bool const starting = action == PowerAction::Start || action == PowerAction::Restart;
    while (!_stopping && std::any_of(watches.begin(), watches.end(), [](Watch const& watch) { return !watch.Settled; }))
    {
        for (Watch& watch : watches)
        {
            if (watch.Settled)
                continue;
            AppSnapshot const now = watch.App->Snapshot();
            std::string outcome;
            std::string message;
            if (starting)
            {
                bool const launched = now.StartedEpochMs != watch.Base.StartedEpochMs;
                bool const ended = now.FailedStarts > watch.Base.FailedStarts || (launched && now.Crashes > watch.Base.Crashes);
                if (launched && now.State == AppState::Running)
                {
                    outcome = "done";
                    message = fmt::format("{} is running as process {}", now.Name, now.ProcessId.value_or(0));
                }
                else if (!Alive(now.State) && (ended || (launched && now.State == AppState::Offline)))
                {
                    outcome = "failed";
                    message = now.Message.empty() ? fmt::format("{} did not become ready", now.Name) : fmt::format("{}: {}", now.Name, now.Message);
                }
            }
            else if (now.State == AppState::Offline || (now.State == AppState::Crashed && !now.WantRunning))
            {
                outcome = "done";
                message = now.Exits.empty() ? fmt::format("{} is down", now.Name) : ManagedApp::DescribeExit(now.Name, now.Exits.back());
            }
            if (outcome.empty() && std::chrono::steady_clock::now() >= watch.Deadline)
            {
                outcome = "failed";
                message = fmt::format("{} did not finish its {} in time and is {}", now.Name, step, ManagedApp::StateName(ManagedApp::Reported(now)));
            }
            if (outcome.empty())
                continue;
            watch.Settled = true;
            Report(report.Operation, now.Name, step, outcome, message);
            if (outcome == "failed")
            {
                succeeded = false;
                failure = message;
            }
        }
        std::this_thread::sleep_for(PollInterval);
    }
    if (_stopping && std::any_of(watches.begin(), watches.end(), [](Watch const& watch) { return !watch.Settled; }))
    {
        failure = "the supervisor stopped before the operation finished";
        return false;
    }
    return succeeded;
}

void PowerOperations::Run(PowerReport report, std::vector<ManagedApp*> apps)
{
    std::vector<ManagedApp*> first;
    std::vector<ManagedApp*> second;
    for (ManagedApp* const app : apps)
        (Tier(app->Snapshot()) == 0 ? first : second).push_back(app);
    std::vector<std::vector<ManagedApp*>> upward;
    for (std::vector<ManagedApp*>* tier : { &first, &second })
        if (!tier->empty())
            upward.push_back(*tier);
    std::vector<std::vector<ManagedApp*>> const downward(upward.rbegin(), upward.rend());

    PowerAction const action = report.Ask.Action;
    uint32 const seconds = report.Ask.Seconds;
    std::string failure;
    bool succeeded = true;
    auto const down = [&](PowerAction how)
    {
        bool leading = true;
        for (std::vector<ManagedApp*> const& tier : downward)
        {
            succeeded = RunTier(report, tier, how, leading ? seconds : 0, failure) && succeeded;
            leading = false;
        }
    };
    auto const up = [&]
    {
        for (std::vector<ManagedApp*> const& tier : upward)
            if (!(succeeded = RunTier(report, tier, PowerAction::Start, 0, failure)))
                return;
    };
    if (action == PowerAction::Start)
        up();
    else if (action == PowerAction::Stop || action == PowerAction::Kill)
        down(action);
    else if (apps.size() == 1)
        succeeded = RunTier(report, apps, PowerAction::Restart, seconds, failure);
    else
    {
        down(PowerAction::Stop);
        if (succeeded)
            up();
    }

    report.Succeeded = succeeded && !_stopping;
    report.Message = report.Succeeded ? fmt::format("{} {} finished", ManagedApp::ActionName(action), DescribeTarget(report.Ask.Target))
        : failure.empty() ? std::string("the supervisor stopped before the operation finished") : failure;
    Release(report.Operation);
    std::lock_guard<std::mutex> const lock(_observerMutex);
    if (_observers.Finished)
        _observers.Finished(report);
}
