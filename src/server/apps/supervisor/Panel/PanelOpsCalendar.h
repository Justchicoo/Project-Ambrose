/*
 * Project Ambrose by Imjustchico
 * The operations calendar 17.96 reads from: one view over the four sources of planned events, schedules, timed game events, installation maintenance and realm maintenance, each carrying its own meaning color, warning where a player-facing entry runs across a disruptive one, each entry opening the page that owns it, and a source whose milestone has not landed reported as unavailable naming that milestone, so the calendar never shows a silent empty where a source is missing.
 */

#ifndef AMBROSE_PANELOPSCALENDAR_H
#define AMBROSE_PANELOPSCALENDAR_H

#include "Types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

class PanelStore;

namespace PanelOpsCalendar
{
    enum class Source : std::uint8_t
    {
        Schedules,
        GameEvents,
        InstallationMaintenance,
        RealmMaintenance,
    };

    constexpr std::string_view SourceName(Source source)
    {
        switch (source)
        {
        case Source::Schedules: return "schedules";
        case Source::GameEvents: return "game_events";
        case Source::InstallationMaintenance: return "installation_maintenance";
        case Source::RealmMaintenance: return "realm_maintenance";
        }
        return "unknown";
    }

    constexpr std::string_view SourceColor(Source source)
    {
        switch (source)
        {
        case Source::Schedules: return "#3b82f6";
        case Source::GameEvents: return "#22c55e";
        case Source::InstallationMaintenance: return "#ef4444";
        case Source::RealmMaintenance: return "#a855f7";
        }
        return "#6b7280";
    }

    constexpr std::string_view SourceMilestone(Source source)
    {
        switch (source)
        {
        case Source::Schedules: return "17.15";
        case Source::GameEvents: return "17.33";
        case Source::InstallationMaintenance: return "17.64";
        case Source::RealmMaintenance: return "17.32";
        }
        return "";
    }

    struct CalendarEvent
    {
        std::string Id;
        Source EventSource = Source::Schedules;
        std::string Title;
        std::string Detail;
        int64 StartEpochMs = 0;
        int64 EndEpochMs = 0;
        bool Disruptive = false;
        std::string Url;
        std::string Permission;
    };

    struct SourceAvailability
    {
        Source EventSource = Source::Schedules;
        bool Available = false;
        std::string UnavailableReason;
    };

    struct CalendarConflict
    {
        std::string EventAId;
        std::string EventBId;
        std::string Reason;
    };

    using EventProvider = std::function<bool(int64 fromEpochMs, int64 toEpochMs, std::vector<CalendarEvent>& events, std::string& error)>;

    struct SourceRegistration
    {
        Source EventSource = Source::Schedules;
        EventProvider Provider;
        std::string RequiredPermission;
    };

    void RegisterDefaults(PanelStore& store, std::vector<SourceRegistration>& registrations);

    bool Collect(std::vector<SourceRegistration> const& registrations, std::vector<std::string> const& viewerPermissions,
        int64 fromEpochMs, int64 toEpochMs, std::vector<CalendarEvent>& events, std::vector<SourceAvailability>& sources,
        std::vector<CalendarConflict>& conflicts, std::string& error);

    std::vector<CalendarConflict> FindConflicts(std::vector<CalendarEvent> const& events);

    bool ViewerMaySee(std::string_view requiredPermission, std::vector<std::string> const& viewerPermissions);

    bool ParseEpochMs(std::string_view text, int64& value);

    int ServerUtcOffsetMinutes();

    nlohmann::json AnswerJson(std::vector<CalendarEvent> const& events, std::vector<SourceAvailability> const& sources,
        std::vector<CalendarConflict> const& conflicts);
}

#endif
