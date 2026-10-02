/*
 * Project Ambrose by Imjustchico
 * Power as tracked operations: a start, stop, restart or kill of one app, one realm's gameservers or the whole stack is given an id, takes a lock on each app it touches and on the stack for a stack target, refuses with 409 naming the holder, its action and its start time when a lock is held, an app is held in a protected state or is disabled, or a restart falls inside the protected hours without an owner's override and its reason, runs on a thread of its own starting the stack in dependency order and stopping it in reverse, and reports each step and the result to its observers; a hold puts an app in a protected state under the same lock until the lease that took it is released, and a kill may pass a held lock.
 */

#ifndef AMBROSE_POWEROPERATIONS_H
#define AMBROSE_POWEROPERATIONS_H

#include "ManagedApp.h"
#include "ProtectedHours.h"
#include "Types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

enum class PowerTargetKind : uint8
{
    App,
    Realm,
    Stack
};

enum class PowerOrigin : uint8
{
    Request,
    Schedule
};

struct PowerTarget
{
    PowerTargetKind Kind = PowerTargetKind::App;
    std::string Name;
};

struct PowerAsk
{
    PowerTarget Target;
    PowerAction Action = PowerAction::Start;
    uint32 Seconds = 0;
    std::string Reason;
    std::string By;
    std::string ActorId;
    PowerOrigin Origin = PowerOrigin::Request;
    bool Override = false;
    std::string RequestId;
};

struct OperationHolder
{
    std::string Id;
    std::string Action;
    std::string By;
    int64 StartedEpochMs = 0;
};

struct PowerStep
{
    std::string Operation;
    std::string App;
    std::string Step;
    std::string Outcome;
    std::string Message;
};

struct PowerReport
{
    std::string Operation;
    PowerAsk Ask;
    std::vector<std::string> Apps;
    bool Succeeded = false;
    std::string Message;
    std::string OverriddenWindow;
};

struct PowerBegun
{
    bool Accepted = false;
    int Status = 202;
    std::string Code;
    std::string Message;
    std::string Operation;
    std::vector<std::string> Apps;
    std::optional<OperationHolder> Holder;
    std::string Window;
};

struct PowerObservers
{
    std::function<void(PowerReport const&)> Accepted;
    std::function<void(PowerStep const&)> Progress;
    std::function<void(PowerReport const&)> Finished;
};

class PowerOperations;

class OperationLease
{
public:
    OperationLease(PowerOperations& owner, ManagedApp& app, OperationHolder holder);
    ~OperationLease();

    OperationLease(OperationLease const&) = delete;
    OperationLease& operator=(OperationLease const&) = delete;

    std::string const& Id() const noexcept { return _holder.Id; }
    void Progress(std::string text);

private:
    PowerOperations& _owner;
    ManagedApp& _app;
    OperationHolder _holder;
};

class PowerOperations
{
public:
    static constexpr std::chrono::milliseconds PollInterval{ 50 };
    static constexpr std::chrono::seconds SettleMargin{ 30 };

    explicit PowerOperations(std::vector<ManagedApp*> apps);
    ~PowerOperations();

    PowerOperations(PowerOperations const&) = delete;
    PowerOperations& operator=(PowerOperations const&) = delete;

    void SetObservers(PowerObservers observers);
    void SetProtectedHours(std::optional<ProtectedHours> hours, std::string problem);
    void SetClock(std::function<std::chrono::system_clock::time_point()> clock);
    void Shutdown();

    PowerBegun Begin(PowerAsk ask);
    std::shared_ptr<OperationLease> Hold(std::string_view app, AppState state, std::string by, PowerBegun& refusal);
    std::optional<OperationHolder> HolderOf(std::string_view app) const;
    std::vector<std::string> Members(PowerTarget const& target, PowerBegun& refusal) const;
    std::optional<std::string> ProtectedRefusal(std::string_view what, PowerOrigin origin, bool overridden, std::string& window) const;

    static std::string_view TargetKindName(PowerTargetKind kind) noexcept;
    static std::optional<PowerTargetKind> ParseTargetKind(std::string_view text) noexcept;
    static std::string DescribeHolder(OperationHolder const& holder);
    static int Tier(AppSnapshot const& snapshot) noexcept;

private:
    friend class OperationLease;

    struct Running
    {
        std::thread Worker;
        std::atomic_bool Done = false;
    };

    std::vector<ManagedApp*> Resolve(PowerTarget const& target, PowerBegun& refusal) const;
    std::optional<OperationHolder> Blocker(std::vector<ManagedApp*> const& apps, bool stack) const;
    void Release(std::string const& id);
    void Run(PowerReport report, std::vector<ManagedApp*> apps);
    bool RunTier(PowerReport const& report, std::vector<ManagedApp*> const& apps, PowerAction action, uint32 seconds, std::string& failure);
    void Report(std::string const& operation, std::string const& app, std::string const& step, std::string const& outcome, std::string const& message);
    void Reap();
    static std::string NewId();
    static int64 NowEpochMs();

    std::vector<ManagedApp*> _apps;
    mutable std::mutex _lockMutex;
    std::map<std::string, OperationHolder, std::less<>> _appLocks;
    std::optional<OperationHolder> _stackLock;
    mutable std::mutex _settingsMutex;
    std::optional<ProtectedHours> _hours;
    std::string _hoursProblem;
    std::function<std::chrono::system_clock::time_point()> _clock;
    std::mutex _observerMutex;
    PowerObservers _observers;
    std::mutex _runningMutex;
    std::vector<std::unique_ptr<Running>> _running;
    std::atomic_bool _stopping = false;
};

#endif
