/*
 * Project Ambrose by Imjustchico
 * The operations calendar 17.96 reads from: the collector over the four registered event sources, the installation maintenance window read live from 17.64's maintenance_state, the schedules, timed game events and realm maintenance sources reporting unavailable naming 17.15, 17.33 and 17.32 until those milestones land and register their providers, entries a viewer may not see filtered by each source's permission, and conflicts where a player-facing entry runs across a disruptive one.
 */

#include "PanelOpsCalendar.h"

#include "PanelMaintenance.h"
#include "PanelStore.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <ctime>
#include <limits>

namespace
{
    constexpr int64 InfiniteEnd = (std::numeric_limits<int64>::max)();

    int64 EffectiveEnd(PanelOpsCalendar::CalendarEvent const& event)
    {
        return event.EndEpochMs == 0 ? InfiniteEnd : event.EndEpochMs;
    }

    bool Overlaps(PanelOpsCalendar::CalendarEvent const& a, PanelOpsCalendar::CalendarEvent const& b)
    {
        return a.StartEpochMs < EffectiveEnd(b) && b.StartEpochMs < EffectiveEnd(a);
    }

    bool InstallationMaintenanceEvents(PanelStore& store, int64 fromEpochMs, int64 toEpochMs,
        std::vector<PanelOpsCalendar::CalendarEvent>& events, std::string& error)
    {
        MaintenanceState state;
        if (!PanelMaintenance::Read(store, state, error))
            return false;
        if (!state.Active)
            return true;
        int64 start = state.WindowStartEpochMs.value_or(state.StartedEpochMs);
        int64 end = state.WindowEndEpochMs.value_or(0);
        if (end != 0 && end < fromEpochMs)
            return true;
        if (start > toEpochMs)
            return true;
        PanelOpsCalendar::CalendarEvent event;
        event.Id = "installation-maintenance";
        event.EventSource = PanelOpsCalendar::Source::InstallationMaintenance;
        event.Title = "Installation maintenance";
        event.Detail = state.Reason;
        event.StartEpochMs = start;
        event.EndEpochMs = end;
        event.Disruptive = true;
        event.Url = "#/overview";
        event.Permission = "status.read";
        events.push_back(std::move(event));
        return true;
    }
}

namespace PanelOpsCalendar
{
    void RegisterDefaults(PanelStore& store, std::vector<SourceRegistration>& registrations)
    {
        registrations.clear();

        SourceRegistration schedules;
        schedules.EventSource = Source::Schedules;
        schedules.RequiredPermission = "schedules.read";
        registrations.push_back(std::move(schedules));

        SourceRegistration gameEvents;
        gameEvents.EventSource = Source::GameEvents;
        gameEvents.RequiredPermission = "events.read";
        registrations.push_back(std::move(gameEvents));

        SourceRegistration installationMaintenance;
        installationMaintenance.EventSource = Source::InstallationMaintenance;
        installationMaintenance.RequiredPermission = "status.read";
        installationMaintenance.Provider = [&store](int64 fromEpochMs, int64 toEpochMs,
                                                std::vector<CalendarEvent>& events, std::string& error)
        {
            return InstallationMaintenanceEvents(store, fromEpochMs, toEpochMs, events, error);
        };
        registrations.push_back(std::move(installationMaintenance));

        SourceRegistration realmMaintenance;
        realmMaintenance.EventSource = Source::RealmMaintenance;
        realmMaintenance.RequiredPermission = "realms.read";
        registrations.push_back(std::move(realmMaintenance));
    }

    bool Collect(std::vector<SourceRegistration> const& registrations, std::vector<std::string> const& viewerPermissions,
        int64 fromEpochMs, int64 toEpochMs, std::vector<CalendarEvent>& events, std::vector<SourceAvailability>& sources,
        std::vector<CalendarConflict>& conflicts, std::string& error)
    {
        events.clear();
        sources.clear();
        conflicts.clear();
        for (SourceRegistration const& registration : registrations)
        {
            SourceAvailability availability;
            availability.EventSource = registration.EventSource;
            if (!registration.Provider)
            {
                availability.Available = false;
                availability.UnavailableReason = std::string(SourceMilestone(registration.EventSource)) + " has not landed";
                sources.push_back(std::move(availability));
                continue;
            }
            std::vector<CalendarEvent> provided;
            if (!registration.Provider(fromEpochMs, toEpochMs, provided, error))
                return false;
            availability.Available = true;
            sources.push_back(std::move(availability));
            for (CalendarEvent& event : provided)
            {
                if (ViewerMaySee(event.Permission.empty() ? registration.RequiredPermission : event.Permission, viewerPermissions))
                    events.push_back(std::move(event));
            }
        }
        conflicts = FindConflicts(events);
        return true;
    }

    std::vector<CalendarConflict> FindConflicts(std::vector<CalendarEvent> const& events)
    {
        std::vector<CalendarConflict> conflicts;
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            for (std::size_t j = i + 1; j < events.size(); ++j)
            {
                CalendarEvent const& a = events[i];
                CalendarEvent const& b = events[j];
                if (a.Disruptive == b.Disruptive)
                    continue;
                if (!Overlaps(a, b))
                    continue;
                CalendarEvent const& disruptive = a.Disruptive ? a : b;
                CalendarEvent const& facing = a.Disruptive ? b : a;
                CalendarConflict conflict;
                conflict.EventAId = facing.Id;
                conflict.EventBId = disruptive.Id;
                conflict.Reason = "\"" + facing.Title + "\" runs across \"" + disruptive.Title + "\"";
                conflicts.push_back(std::move(conflict));
            }
        }
        return conflicts;
    }

    bool ViewerMaySee(std::string_view requiredPermission, std::vector<std::string> const& viewerPermissions)
    {
        if (requiredPermission.empty())
            return true;
        for (std::string const& held : viewerPermissions)
        {
            if (held == requiredPermission)
                return true;
        }
        return false;
    }

    bool ParseEpochMs(std::string_view text, int64& value)
    {
        if (text.empty())
            return false;
        int64 parsed = 0;
        auto [end, code] = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (code != std::errc() || end != text.data() + text.size() || parsed < 0)
            return false;
        value = parsed;
        return true;
    }

    int ServerUtcOffsetMinutes()
    {
        std::time_t now = std::time(nullptr);
#if defined(_WIN32)
        long biasSeconds = 0;
        _get_timezone(&biasSeconds);
        return static_cast<int>(-biasSeconds / 60);
#else
        std::tm broken{};
        localtime_r(&now, &broken);
        return static_cast<int>(broken.tm_gmtoff / 60);
#endif
    }

    nlohmann::json AnswerJson(std::vector<CalendarEvent> const& events, std::vector<SourceAvailability> const& sources,
        std::vector<CalendarConflict> const& conflicts)
    {
        nlohmann::json answer = nlohmann::json::object();
        nlohmann::json list = nlohmann::json::array();
        for (CalendarEvent const& event : events)
        {
            nlohmann::json item = nlohmann::json::object();
            item["id"] = event.Id;
            item["source"] = SourceName(event.EventSource);
            item["color"] = SourceColor(event.EventSource);
            item["title"] = event.Title;
            item["detail"] = event.Detail;
            item["start_epoch_ms"] = event.StartEpochMs;
            item["end_epoch_ms"] = event.EndEpochMs;
            item["disruptive"] = event.Disruptive;
            item["url"] = event.Url;
            list.push_back(std::move(item));
        }
        answer["events"] = std::move(list);
        nlohmann::json availability = nlohmann::json::array();
        for (SourceAvailability const& source : sources)
        {
            nlohmann::json item = nlohmann::json::object();
            item["source"] = SourceName(source.EventSource);
            item["color"] = SourceColor(source.EventSource);
            item["available"] = source.Available;
            if (!source.Available)
                item["unavailable_reason"] = source.UnavailableReason;
            availability.push_back(std::move(item));
        }
        answer["sources"] = std::move(availability);
        nlohmann::json found = nlohmann::json::array();
        for (CalendarConflict const& conflict : conflicts)
        {
            nlohmann::json item = nlohmann::json::object();
            item["event_a_id"] = conflict.EventAId;
            item["event_b_id"] = conflict.EventBId;
            item["reason"] = conflict.Reason;
            found.push_back(std::move(item));
        }
        answer["conflicts"] = std::move(found);
        answer["server_utc_offset_minutes"] = ServerUtcOffsetMinutes();
        return answer;
    }
}
