/*
 * Project Ambrose by Imjustchico
 * Every start, stop, restart and kill as one tracked operation with a visible result that never overlaps with a restore, update or rebuild. A lock per app and a stack lock refuse a second operation with 409 naming the holder, its action and start time, while kill may bypass a held lock after marking the app stopping. Power targets reach one app, one realm's gameservers or the whole stack, started in dependency order and stopped in reverse. Each operation answers 202 with an id, streams its steps and result as power progress and result events on the event socket, and lands in the audit log. Disabling an app stops it and refuses starts while disabled, with the reason and who set it on the record. Protected hours name the window when a restart, update or migration is refused, with an owner override that needs a reason and writes an audit row.
 */

#ifndef AMBROSE_PANELPOWER_H
#define AMBROSE_PANELPOWER_H

#include "Types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PowerTargetKind : uint8
{
    App,
    Realm,
    Stack
};

enum class PowerOperationState : uint8
{
    Running,
    Done,
    Failed,
    Refused
};

struct PowerOperationLock
{
    std::string Holder;
    std::string Action;
    int64 StartEpochMs = 0;
    std::string Target;
};

struct PowerOperation
{
    std::string Id;
    PowerTargetKind TargetKind = PowerTargetKind::App;
    std::string Target;
    std::string Action;
    std::string RequestedBy;
    int64 StartEpochMs = 0;
    std::optional<int64> EndEpochMs;
    PowerOperationState State = PowerOperationState::Running;
    std::string Result;
    std::vector<std::string> Steps;
};

struct PowerDisableRecord
{
    std::string App;
    std::string Reason;
    std::string SetBy;
    int64 SetEpochMs = 0;
};

class PanelPower
{
public:
    using Clock = std::chrono::steady_clock;

    PanelPower();

    PanelPower(PanelPower const&) = delete;
    PanelPower& operator=(PanelPower const&) = delete;

    static std::string_view TargetKindName(PowerTargetKind kind) noexcept;
    static std::string_view OperationStateName(PowerOperationState state) noexcept;
    static bool IsProtectedState(std::string_view state) noexcept;

    bool TryAcquireAppLock(std::string_view app, std::string_view holder, std::string_view action, std::string_view target, PowerOperationLock& existing);
    bool TryAcquireStackLock(std::string_view holder, std::string_view action, std::string_view target, PowerOperationLock& existing);
    void ReleaseAppLock(std::string_view app);
    void ReleaseStackLock();
    void ReleaseAllFor(std::string_view holder);
    bool KillBypassAppLock(std::string_view app, std::string_view holder);

    std::string BeginOperation(PowerTargetKind kind, std::string_view target, std::string_view action, std::string_view requestedBy);
    void AddStep(std::string_view operationId, std::string_view step);
    void FinishOperation(std::string_view operationId, bool ok, std::string_view result);
    std::optional<PowerOperation> FindOperation(std::string_view id) const;
    std::vector<PowerOperation> ListOperations() const;

    bool DisableApp(std::string_view app, std::string_view reason, std::string_view setBy, std::string& error);
    bool EnableApp(std::string_view app, std::string& error);
    std::optional<PowerDisableRecord> DisableInfo(std::string_view app) const;
    bool IsDisabled(std::string_view app) const;

    static bool ParseProtectedHours(std::string_view text, uint8& fromHour, uint8& toHour, std::string& error);
    static bool InProtectedHours(uint8 fromHour, uint8 toHour, uint8 nowHour) noexcept;

private:
    static int64 NowEpochMs() noexcept;

    mutable std::mutex _mutex;
    std::map<std::string, PowerOperationLock, std::less<>> _appLocks;
    std::optional<PowerOperationLock> _stackLock;
    std::map<std::string, PowerOperation, std::less<>> _operations;
    std::map<std::string, PowerDisableRecord, std::less<>> _disabled;
    uint64 _nextId = 1;
};

#endif
