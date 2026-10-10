/*
 * Project Ambrose by Imjustchico
 * The installation maintenance record 17.64's panel banner and control read and write: one row in the panel store saying whether maintenance is on, who turned it on, why, when it started and the optional window the public status page (17.70) will publish, entered and left through the loginserver's live Login.Maintenance and Login.MaintenanceReason settings in one batch so the switch and the reason change together, every change in the same transaction as its audit row with who, why and the window, so a change that is not recorded is not applied either.
 */

#include "PanelMaintenance.h"

#include "PanelAudit.h"
#include "PanelStore.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

namespace
{
    bool Write(PanelStore& store, MaintenanceState const& state, std::string& error)
    {
        std::optional<PanelStore::Statement> write = store.Prepare(
            "INSERT INTO maintenance_state (id, active, reason, started_by, started_epoch_ms, window_start_epoch_ms, window_end_epoch_ms) "
            "VALUES (1, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET active = excluded.active, reason = excluded.reason, started_by = excluded.started_by, "
            "started_epoch_ms = excluded.started_epoch_ms, window_start_epoch_ms = excluded.window_start_epoch_ms, window_end_epoch_ms = excluded.window_end_epoch_ms",
            error);
        if (!write)
            return false;
        write->Bind(1, state.Active ? int64(1) : int64(0));
        write->Bind(2, state.Reason);
        write->Bind(3, state.StartedBy);
        write->Bind(4, state.StartedEpochMs);
        if (state.WindowStartEpochMs)
            write->Bind(5, *state.WindowStartEpochMs);
        else
            write->BindNull(5);
        if (state.WindowEndEpochMs)
            write->Bind(6, *state.WindowEndEpochMs);
        else
            write->BindNull(6);
        return write->Run(error);
    }

    bool CheckReason(std::string_view reason, std::string& clean, std::string& error)
    {
        clean = std::string(Ambrose::Trim(reason));
        if (clean.empty())
        {
            error = "entering maintenance takes a reason";
            return false;
        }
        if (clean.size() > PanelMaintenance::MaxReasonBytes)
        {
            error = "the reason is longer than 255 bytes";
            return false;
        }
        return true;
    }

    bool CheckWindow(std::optional<int64> start, std::optional<int64> end, std::string& error)
    {
        if (start.has_value() != end.has_value())
        {
            error = "a maintenance window takes both its start and its end";
            return false;
        }
        if (start && end && *start >= *end)
        {
            error = "the window starts after it ends";
            return false;
        }
        return true;
    }

    nlohmann::json WindowProperties(std::optional<int64> start, std::optional<int64> end)
    {
        nlohmann::json properties = nlohmann::json::object();
        properties["window_start_epoch_ms"] = start ? nlohmann::json(*start) : nlohmann::json();
        properties["window_end_epoch_ms"] = end ? nlohmann::json(*end) : nlohmann::json();
        return properties;
    }
}

namespace PanelMaintenance
{
    bool Read(PanelStore& store, MaintenanceState& state, std::string& error)
    {
        state = MaintenanceState{};
        std::optional<PanelStore::Statement> rows = store.Prepare(
            "SELECT active, reason, started_by, started_epoch_ms, window_start_epoch_ms, window_end_epoch_ms FROM maintenance_state WHERE id = 1", error);
        if (!rows)
            return false;
        if (!rows->Step(error))
            return error.empty();
        state.Active = rows->Int64(0) != 0;
        state.Reason = rows->Text(1);
        state.StartedBy = rows->Text(2);
        state.StartedEpochMs = rows->Int64(3);
        if (!rows->IsNull(4))
            state.WindowStartEpochMs = rows->Int64(4);
        if (!rows->IsNull(5))
            state.WindowEndEpochMs = rows->Int64(5);
        return true;
    }

    nlohmann::json Answer(MaintenanceState const& state)
    {
        nlohmann::json answer{
            { "active", state.Active },
            { "reason", state.Reason },
            { "started_by", state.StartedBy },
            { "started_epoch_ms", state.StartedEpochMs },
            { "window_start_epoch_ms", state.WindowStartEpochMs ? nlohmann::json(*state.WindowStartEpochMs) : nlohmann::json() },
            { "window_end_epoch_ms", state.WindowEndEpochMs ? nlohmann::json(*state.WindowEndEpochMs) : nlohmann::json() },
        };
        return answer;
    }

    std::string BatchBody(std::string_view reason, bool active, std::string_view changeReason)
    {
        nlohmann::json body{
            { "reason", changeReason },
            { "entries",
                {
                    { { "key", "Login.Maintenance" }, { "value", active ? "1" : "0" } },
                    { { "key", "Login.MaintenanceReason" }, { "value", std::string(reason) } },
                } },
        };
        return body.dump();
    }

    bool Enter(PanelStore& store, std::mutex& storeMutex, MaintenanceAppCall const& call, bool queueForCollector, AuditActor actor, std::string_view actorId,
        std::string_view actorName, std::string_view address, std::string_view userAgent, std::string_view reason, std::optional<int64> windowStartEpochMs,
        std::optional<int64> windowEndEpochMs, MaintenanceState& state, std::string& error)
    {
        std::string clean;
        if (!CheckReason(reason, clean, error))
            return false;
        if (!CheckWindow(windowStartEpochMs, windowEndEpochMs, error))
            return false;

        MaintenanceAppCallResult const applied = call("loginserver", "POST", "/api/settings/batch", BatchBody(clean, true, "maintenance entered by " + std::string(actorName)));
        if (!applied.Ok)
        {
            error = applied.Error.empty() ? "the loginserver did not answer" : applied.Error;
            return false;
        }
        if (applied.Status != 200)
        {
            error = "the loginserver refused the maintenance switch with status " + std::to_string(applied.Status);
            return false;
        }

        MaintenanceState next;
        next.Active = true;
        next.Reason = clean;
        next.StartedBy = std::string(actorName);
        next.StartedEpochMs = PanelStore::NowEpochMs();
        next.WindowStartEpochMs = windowStartEpochMs;
        next.WindowEndEpochMs = windowEndEpochMs;

        AuditEvent event;
        event.Name = std::string(EnteredEvent);
        event.Actor = actor;
        event.ActorId = std::string(actorId);
        event.ActorName = std::string(actorName);
        event.Address = std::string(address);
        event.UserAgent = std::string(userAgent);
        event.Reason = clean;
        event.Properties = WindowProperties(windowStartEpochMs, windowEndEpochMs).dump();
        event.On("app", "loginserver", "loginserver");
        std::lock_guard const lock(storeMutex);
        if (!store.IsOpen())
        {
            error = "the panel store is not open, so the maintenance record was not written";
            return false;
        }
        if (!PanelAudit::Record(store, event, [&](std::string& failure) { return Write(store, next, failure); }, error, queueForCollector))
            return false;
        state = std::move(next);
        return true;
    }

    bool Exit(PanelStore& store, std::mutex& storeMutex, MaintenanceAppCall const& call, bool queueForCollector, AuditActor actor, std::string_view actorId,
        std::string_view actorName, std::string_view address, std::string_view userAgent, MaintenanceState& state, std::string& error)
    {
        MaintenanceAppCallResult const applied = call("loginserver", "POST", "/api/settings/batch", BatchBody("", false, "maintenance left by " + std::string(actorName)));
        if (!applied.Ok)
        {
            error = applied.Error.empty() ? "the loginserver did not answer" : applied.Error;
            return false;
        }
        if (applied.Status != 200)
        {
            error = "the loginserver refused the maintenance switch with status " + std::to_string(applied.Status);
            return false;
        }

        MaintenanceState next;

        AuditEvent event;
        event.Name = std::string(ExitedEvent);
        event.Actor = actor;
        event.ActorId = std::string(actorId);
        event.ActorName = std::string(actorName);
        event.Address = std::string(address);
        event.UserAgent = std::string(userAgent);
        event.Properties = WindowProperties(std::nullopt, std::nullopt).dump();
        event.On("app", "loginserver", "loginserver");
        std::lock_guard const lock(storeMutex);
        if (!store.IsOpen())
        {
            error = "the panel store is not open, so the maintenance record was not written";
            return false;
        }
        if (!PanelAudit::Record(store, event, [&](std::string& failure) { return Write(store, next, failure); }, error, queueForCollector))
            return false;
        state = std::move(next);
        return true;
    }
}
