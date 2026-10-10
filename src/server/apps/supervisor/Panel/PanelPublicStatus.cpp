/*
 * Project Ambrose by Imjustchico
 * The public status page 17.70 reads and serves: one cached read-only answer saying which realms are up, whether the login server takes players and the maintenance window 17.64 published, plus the operator's incident note, shaped so the payload carries no player name, address, account figure or app internal, with the incident note posted and cleared under the panel.status permission and audited with who and when.
 */

#include "PanelPublicStatus.h"

#include "PanelMaintenance.h"
#include "PanelStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace
{
    bool WriteIncident(PanelStore& store, PublicIncident const& incident, std::string& error)
    {
        std::optional<PanelStore::Statement> write = store.Prepare(
            "UPDATE public_status_incident SET active = ?, note = ?, posted_by = ?, posted_epoch_ms = ? WHERE id = 1", error);
        if (!write)
            return false;
        write->Bind(1, incident.Active ? int64(1) : int64(0));
        write->Bind(2, incident.Note);
        write->Bind(3, incident.PostedBy);
        write->Bind(4, incident.PostedEpochMs);
        return write->Run(error);
    }

    bool ShapedObject(nlohmann::json const& value, std::vector<std::string> const& allowed, std::string& offending)
    {
        if (!value.is_object())
        {
            offending = "expected an object";
            return false;
        }
        for (auto const& [key, _] : value.items())
        {
            if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            {
                offending = key;
                return false;
            }
        }
        return true;
    }
}

PublicStatusCache::PublicStatusCache(std::chrono::seconds ttl)
    : _ttl(ttl)
{
}

std::optional<std::string> PublicStatusCache::Get() const
{
    std::lock_guard const lock(_mutex);
    if (!_has)
        return std::nullopt;
    if (std::chrono::steady_clock::now() - _at > _ttl)
        return std::nullopt;
    return _body;
}

void PublicStatusCache::Put(std::string body)
{
    std::lock_guard const lock(_mutex);
    _body = std::move(body);
    _at = std::chrono::steady_clock::now();
    _has = true;
}

void PublicStatusCache::Clear()
{
    std::lock_guard const lock(_mutex);
    _body.clear();
    _has = false;
}

namespace PanelPublicStatus
{
    bool ReadIncident(PanelStore& store, PublicIncident& incident, std::string& error)
    {
        incident = PublicIncident{};
        std::optional<PanelStore::Statement> rows = store.Prepare(
            "SELECT active, note, posted_by, posted_epoch_ms FROM public_status_incident WHERE id = 1", error);
        if (!rows)
            return false;
        if (!rows->Step(error))
            return error.empty();
        incident.Active = rows->Int64(0) != 0;
        incident.Note = rows->Text(1);
        incident.PostedBy = rows->Text(2);
        incident.PostedEpochMs = rows->Int64(3);
        return true;
    }

    nlohmann::json Answer(PublicSourceState const& source, MaintenanceState const& maintenance,
        PublicIncident const& incident, int64 nowEpochMs)
    {
        nlohmann::json realms = nlohmann::json::array();
        for (PublicRealmStatus const& realm : source.Realms)
            realms.push_back({ { "name", realm.Name }, { "up", realm.Up } });
        nlohmann::json answer{
            { "realms", std::move(realms) },
            { "login_accepting_players", source.LoginServerUp && !maintenance.Active },
            { "maintenance",
                {
                    { "active", maintenance.Active },
                    { "window_start_epoch_ms",
                        maintenance.WindowStartEpochMs ? nlohmann::json(*maintenance.WindowStartEpochMs) : nlohmann::json() },
                    { "window_end_epoch_ms",
                        maintenance.WindowEndEpochMs ? nlohmann::json(*maintenance.WindowEndEpochMs) : nlohmann::json() },
                } },
            { "incident", incident.Active ? nlohmann::json({ { "note", incident.Note } }) : nlohmann::json() },
            { "generated_epoch_ms", nowEpochMs },
        };
        return answer;
    }

    bool CheckShaped(nlohmann::json const& payload, std::string& offending)
    {
        offending.clear();
        if (!ShapedObject(payload, { "realms", "login_accepting_players", "maintenance", "incident", "generated_epoch_ms" }, offending))
            return false;
        for (nlohmann::json const& realm : payload["realms"])
        {
            if (!ShapedObject(realm, { "name", "up" }, offending))
                return false;
            if (!realm["name"].is_string() || !realm["up"].is_boolean())
            {
                offending = "realm";
                return false;
            }
        }
        if (!payload["login_accepting_players"].is_boolean() || !payload["generated_epoch_ms"].is_number_integer())
        {
            offending = "scalar";
            return false;
        }
        if (!ShapedObject(payload["maintenance"], { "active", "window_start_epoch_ms", "window_end_epoch_ms" }, offending))
            return false;
        if (!payload["maintenance"]["active"].is_boolean())
        {
            offending = "maintenance.active";
            return false;
        }
        for (char const* key : { "window_start_epoch_ms", "window_end_epoch_ms" })
        {
            nlohmann::json const& window = payload["maintenance"][key];
            if (!window.is_null() && !window.is_number_integer())
            {
                offending = key;
                return false;
            }
        }
        nlohmann::json const& incident = payload["incident"];
        if (!incident.is_null() && !ShapedObject(incident, { "note" }, offending))
            return false;
        if (incident.is_object() && !incident["note"].is_string())
        {
            offending = "incident.note";
            return false;
        }
        return true;
    }

    bool PostIncident(PanelStore& store, std::mutex& storeMutex, bool queueForCollector, AuditActor actor,
        std::string_view actorId, std::string_view actorName, std::string_view address, std::string_view userAgent,
        std::string_view note, PublicIncident& incident, std::string& error)
    {
        if (note.empty() || note.size() > MaxNoteBytes)
        {
            error = "the incident note is empty or longer than 500 bytes";
            return false;
        }
        PublicIncident next;
        next.Active = true;
        next.Note = std::string(note);
        next.PostedBy = std::string(actorName);
        next.PostedEpochMs = PanelStore::NowEpochMs();

        AuditEvent event;
        event.Name = std::string(IncidentPostedEvent);
        event.Actor = actor;
        event.ActorId = std::string(actorId);
        event.ActorName = std::string(actorName);
        event.Address = std::string(address);
        event.UserAgent = std::string(userAgent);
        event.Reason = next.Note;
        std::lock_guard const lock(storeMutex);
        if (!store.IsOpen())
        {
            error = "the panel store is not open, so the incident note was not written";
            return false;
        }
        if (!PanelAudit::Record(store, event, [&](std::string& failure) { return WriteIncident(store, next, failure); }, error, queueForCollector))
            return false;
        incident = std::move(next);
        return true;
    }

    bool ClearIncident(PanelStore& store, std::mutex& storeMutex, bool queueForCollector, AuditActor actor,
        std::string_view actorId, std::string_view actorName, std::string_view address, std::string_view userAgent,
        PublicIncident& incident, std::string& error)
    {
        PublicIncident next;

        AuditEvent event;
        event.Name = std::string(IncidentClearedEvent);
        event.Actor = actor;
        event.ActorId = std::string(actorId);
        event.ActorName = std::string(actorName);
        event.Address = std::string(address);
        event.UserAgent = std::string(userAgent);
        std::lock_guard const lock(storeMutex);
        if (!store.IsOpen())
        {
            error = "the panel store is not open, so the incident note was not cleared";
            return false;
        }
        if (!PanelAudit::Record(store, event, [&](std::string& failure) { return WriteIncident(store, next, failure); }, error, queueForCollector))
            return false;
        incident = std::move(next);
        return true;
    }
}
