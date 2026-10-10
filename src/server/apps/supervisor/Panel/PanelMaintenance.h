/*
 * Project Ambrose by Imjustchico
 * The installation maintenance record 17.64's panel banner and control read and write: one row in the panel store saying whether maintenance is on, who turned it on, why, when it started and the optional window the public status page (17.70) will publish, entered and left through the loginserver's live Login.Maintenance and Login.MaintenanceReason settings in one batch so the switch and the reason change together, every change in the same transaction as its audit row with who, why and the window, so a change that is not recorded is not applied either.
 */

#ifndef AMBROSE_PANELMAINTENANCE_H
#define AMBROSE_PANELMAINTENANCE_H

#include "Types.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "PanelAudit.h"

class PanelStore;

struct MaintenanceState
{
    bool Active = false;
    std::string Reason;
    std::string StartedBy;
    int64 StartedEpochMs = 0;
    std::optional<int64> WindowStartEpochMs;
    std::optional<int64> WindowEndEpochMs;
};

struct MaintenanceAppCallResult
{
    bool Ok = false;
    int Status = 0;
    std::string Body;
    std::string Error;
};

using MaintenanceAppCall = std::function<MaintenanceAppCallResult(std::string_view app, std::string_view method, std::string_view path, std::string_view body)>;

namespace PanelMaintenance
{
    constexpr std::string_view EnteredEvent = "maintenance:entered";
    constexpr std::string_view ExitedEvent = "maintenance:exited";
    constexpr std::size_t MaxReasonBytes = 255;

    bool Read(PanelStore& store, MaintenanceState& state, std::string& error);
    nlohmann::json Answer(MaintenanceState const& state);
    std::string BatchBody(std::string_view reason, bool active, std::string_view changeReason);

    bool Enter(PanelStore& store, std::mutex& storeMutex, MaintenanceAppCall const& call, bool queueForCollector, AuditActor actor, std::string_view actorId,
        std::string_view actorName, std::string_view address, std::string_view userAgent, std::string_view reason, std::optional<int64> windowStartEpochMs,
        std::optional<int64> windowEndEpochMs, MaintenanceState& state, std::string& error);
    bool Exit(PanelStore& store, std::mutex& storeMutex, MaintenanceAppCall const& call, bool queueForCollector, AuditActor actor, std::string_view actorId,
        std::string_view actorName, std::string_view address, std::string_view userAgent, MaintenanceState& state, std::string& error);
}

#endif
