/*
 * Project Ambrose by Imjustchico
 * The public status page 17.70 reads and serves: one cached read-only answer saying which realms are up, whether the login server takes players and the maintenance window 17.64 published, plus the operator's incident note, shaped so the payload carries no player name, address, account figure or app internal, with the incident note posted and cleared under the panel.status permission and audited with who and when.
 */

#ifndef AMBROSE_PANELPUBLICSTATUS_H
#define AMBROSE_PANELPUBLICSTATUS_H

#include "Types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "PanelAudit.h"

class PanelStore;
struct MaintenanceState;

struct PublicRealmStatus
{
    std::string Name;
    bool Up = false;
};

struct PublicSourceState
{
    std::vector<PublicRealmStatus> Realms;
    bool LoginServerUp = false;
};

using PublicStatusSource = std::function<PublicSourceState()>;

struct PublicIncident
{
    bool Active = false;
    std::string Note;
    std::string PostedBy;
    int64 PostedEpochMs = 0;
};

class PublicStatusCache
{
public:
    explicit PublicStatusCache(std::chrono::seconds ttl = std::chrono::seconds(30));

    std::optional<std::string> Get() const;
    void Put(std::string body);
    void Clear();

private:
    mutable std::mutex _mutex;
    std::chrono::seconds _ttl;
    std::string _body;
    std::chrono::steady_clock::time_point _at{};
    bool _has = false;
};

namespace PanelPublicStatus
{
    constexpr std::string_view IncidentPostedEvent = "publicstatus:incident.posted";
    constexpr std::string_view IncidentClearedEvent = "publicstatus:incident.cleared";
    constexpr std::size_t MaxNoteBytes = 500;
    constexpr uint32 StatusCost = 5;

    bool ReadIncident(PanelStore& store, PublicIncident& incident, std::string& error);
    nlohmann::json Answer(PublicSourceState const& source, MaintenanceState const& maintenance,
        PublicIncident const& incident, int64 nowEpochMs);
    bool CheckShaped(nlohmann::json const& payload, std::string& offending);
    bool PostIncident(PanelStore& store, std::mutex& storeMutex, bool queueForCollector, AuditActor actor,
        std::string_view actorId, std::string_view actorName, std::string_view address, std::string_view userAgent,
        std::string_view note, PublicIncident& incident, std::string& error);
    bool ClearIncident(PanelStore& store, std::mutex& storeMutex, bool queueForCollector, AuditActor actor,
        std::string_view actorId, std::string_view actorName, std::string_view address, std::string_view userAgent,
        PublicIncident& incident, std::string& error);
}

#endif
