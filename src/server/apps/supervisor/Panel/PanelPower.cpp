/*
 * Project Ambrose by Imjustchico
 * Every start, stop, restart and kill as one tracked operation with a visible result that never overlaps with a restore, update or rebuild. A lock per app and a stack lock refuse a second operation with 409 naming the holder, its action and start time, while kill may bypass a held lock after marking the app stopping. Power targets reach one app, one realm's gameservers or the whole stack, started in dependency order and stopped in reverse. Each operation answers 202 with an id, streams its steps and result as power progress and result events on the event socket, and lands in the audit log. Disabling an app stops it and refuses starts while disabled, with the reason and who set it on the record. Protected hours name the window when a restart, update or migration is refused, with an owner override that needs a reason and writes an audit row.
 */

#include "PanelPower.h"

#include <charconv>
#include <cstdio>

PanelPower::PanelPower() = default;

std::string_view PanelPower::TargetKindName(PowerTargetKind kind) noexcept
{
    switch (kind)
    {
        case PowerTargetKind::App: return "app";
        case PowerTargetKind::Realm: return "realm";
        case PowerTargetKind::Stack: return "stack";
    }
    return "app";
}

std::string_view PanelPower::OperationStateName(PowerOperationState state) noexcept
{
    switch (state)
    {
        case PowerOperationState::Running: return "running";
        case PowerOperationState::Done: return "done";
        case PowerOperationState::Failed: return "failed";
        case PowerOperationState::Refused: return "refused";
    }
    return "running";
}

bool PanelPower::IsProtectedState(std::string_view state) noexcept
{
    return state == "setup" || state == "updating" || state == "restoring" || state == "moving";
}

int64 PanelPower::NowEpochMs() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool PanelPower::TryAcquireAppLock(std::string_view app, std::string_view holder, std::string_view action, std::string_view target, PowerOperationLock& existing)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _appLocks.find(app);
    if (it != _appLocks.end())
    {
        existing = it->second;
        return false;
    }
    if (_stackLock)
    {
        existing = *_stackLock;
        return false;
    }
    PowerOperationLock lock;
    lock.Holder = std::string(holder);
    lock.Action = std::string(action);
    lock.StartEpochMs = NowEpochMs();
    lock.Target = std::string(target);
    _appLocks.emplace(std::string(app), std::move(lock));
    return true;
}

bool PanelPower::TryAcquireStackLock(std::string_view holder, std::string_view action, std::string_view target, PowerOperationLock& existing)
{
    std::lock_guard<std::mutex> guard(_mutex);
    if (_stackLock)
    {
        existing = *_stackLock;
        return false;
    }
    if (!_appLocks.empty())
    {
        existing = _appLocks.begin()->second;
        return false;
    }
    PowerOperationLock lock;
    lock.Holder = std::string(holder);
    lock.Action = std::string(action);
    lock.StartEpochMs = NowEpochMs();
    lock.Target = std::string(target);
    _stackLock = std::move(lock);
    return true;
}

void PanelPower::ReleaseAppLock(std::string_view app)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _appLocks.erase(std::string(app));
}

void PanelPower::ReleaseStackLock()
{
    std::lock_guard<std::mutex> guard(_mutex);
    _stackLock.reset();
}

void PanelPower::ReleaseAllFor(std::string_view holder)
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (auto it = _appLocks.begin(); it != _appLocks.end();)
    {
        if (it->second.Holder == holder)
            it = _appLocks.erase(it);
        else
            ++it;
    }
    if (_stackLock && _stackLock->Holder == holder)
        _stackLock.reset();
}

bool PanelPower::KillBypassAppLock(std::string_view app, std::string_view holder)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _appLocks.find(app);
    if (it == _appLocks.end())
        return true;
    PowerOperationLock lock;
    lock.Holder = std::string(holder);
    lock.Action = "kill";
    lock.StartEpochMs = NowEpochMs();
    lock.Target = it->second.Target;
    it->second = std::move(lock);
    return true;
}

std::string PanelPower::BeginOperation(PowerTargetKind kind, std::string_view target, std::string_view action, std::string_view requestedBy)
{
    std::lock_guard<std::mutex> guard(_mutex);
    char id[32];
    std::snprintf(id, sizeof(id), "pow-%llu", static_cast<unsigned long long>(_nextId++));
    PowerOperation op;
    op.Id = id;
    op.TargetKind = kind;
    op.Target = std::string(target);
    op.Action = std::string(action);
    op.RequestedBy = std::string(requestedBy);
    op.StartEpochMs = NowEpochMs();
    op.State = PowerOperationState::Running;
    _operations.emplace(op.Id, op);
    return op.Id;
}

void PanelPower::AddStep(std::string_view operationId, std::string_view step)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _operations.find(operationId);
    if (it != _operations.end())
        it->second.Steps.emplace_back(step);
}

void PanelPower::FinishOperation(std::string_view operationId, bool ok, std::string_view result)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _operations.find(operationId);
    if (it != _operations.end())
    {
        it->second.State = ok ? PowerOperationState::Done : PowerOperationState::Failed;
        it->second.EndEpochMs = NowEpochMs();
        it->second.Result = std::string(result);
    }
}

std::optional<PowerOperation> PanelPower::FindOperation(std::string_view id) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _operations.find(id);
    if (it == _operations.end())
        return std::nullopt;
    return it->second;
}

std::vector<PowerOperation> PanelPower::ListOperations() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    std::vector<PowerOperation> out;
    out.reserve(_operations.size());
    for (auto const& [id, op] : _operations)
        out.push_back(op);
    return out;
}

bool PanelPower::DisableApp(std::string_view app, std::string_view reason, std::string_view setBy, std::string& error)
{
    if (reason.empty())
    {
        error = "A reason is required to disable an app";
        return false;
    }
    std::lock_guard<std::mutex> guard(_mutex);
    PowerDisableRecord record;
    record.App = std::string(app);
    record.Reason = std::string(reason);
    record.SetBy = std::string(setBy);
    record.SetEpochMs = NowEpochMs();
    _disabled[record.App] = std::move(record);
    return true;
}

bool PanelPower::EnableApp(std::string_view app, std::string& error)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _disabled.find(app);
    if (it == _disabled.end())
    {
        error = "The app is not disabled";
        return false;
    }
    _disabled.erase(it);
    return true;
}

std::optional<PowerDisableRecord> PanelPower::DisableInfo(std::string_view app) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _disabled.find(app);
    if (it == _disabled.end())
        return std::nullopt;
    return it->second;
}

bool PanelPower::IsDisabled(std::string_view app) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _disabled.find(app) != _disabled.end();
}

bool PanelPower::ParseProtectedHours(std::string_view text, uint8& fromHour, uint8& toHour, std::string& error)
{
    auto dash = text.find('-');
    if (dash == std::string_view::npos)
    {
        error = "Protected hours look like 02-06";
        return false;
    }
    std::string_view fromText = text.substr(0, dash);
    std::string_view toText = text.substr(dash + 1);
    unsigned from = 0, to = 0;
    auto fromEnd = fromText.data() + fromText.size();
    auto toEnd = toText.data() + toText.size();
    auto fromResult = std::from_chars(fromText.data(), fromEnd, from);
    auto toResult = std::from_chars(toText.data(), toEnd, to);
    if (fromResult.ec != std::errc() || fromResult.ptr != fromEnd || from > 23)
    {
        error = "The start hour must be 0-23";
        return false;
    }
    if (toResult.ec != std::errc() || toResult.ptr != toEnd || to > 23)
    {
        error = "The end hour must be 0-23";
        return false;
    }
    fromHour = static_cast<uint8>(from);
    toHour = static_cast<uint8>(to);
    return true;
}

bool PanelPower::InProtectedHours(uint8 fromHour, uint8 toHour, uint8 nowHour) noexcept
{
    if (fromHour == toHour)
        return false;
    if (fromHour < toHour)
        return nowHour >= fromHour && nowHour < toHour;
    return nowHour >= fromHour || nowHour < toHour;
}
