/*
 * Project Ambrose by Imjustchico
 * Uptime history and incident timeline 17.94: per-realm and login-path uptime percentages over a day, a month and a year computed from the probe's own samples, the incident timeline with planned maintenance windows marked as planned, per-realm sparklines, and the aggregate-only shaping of the extended public and operator payloads.
 */

#ifndef AMBROSE_PANELUPTIMEHISTORY_H
#define AMBROSE_PANELUPTIMEHISTORY_H

#include "Types.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

struct ProbeSample
{
    int64 EpochMs = 0;
    std::string Target;
    bool Ok = false;
};

using ProbeSampleSource = std::function<std::vector<ProbeSample>()>;

struct UptimeFigure
{
    bool Known = false;
    double Percent = 0.0;
};

struct UptimeWindows
{
    UptimeFigure Day;
    UptimeFigure Month;
    UptimeFigure Year;
};

struct UptimeSummary
{
    std::map<std::string, UptimeWindows> Realms;
    UptimeWindows Login;
    int64 ComputedEpochMs = 0;
};

struct PlannedWindow
{
    int64 StartEpochMs = 0;
    int64 EndEpochMs = 0;
    std::string Reason;
};

struct TimelineIncident
{
    int64 StartEpochMs = 0;
    int64 EndEpochMs = 0;
    std::string Cause;
    bool Planned = false;
    std::string Note;
};

struct RealmSparkline
{
    std::string Realm;
    std::vector<std::optional<double>> Buckets;
};

namespace PanelUptimeHistory
{
    constexpr std::string_view LoginTarget = "login";
    constexpr int SparklineBuckets = 24;
    constexpr int64 MsPerHour = 3600000;
    constexpr int64 MsPerDay = 86400000;
    constexpr int64 MsPerMonth = 2592000000;
    constexpr int64 MsPerYear = 31536000000;

    UptimeSummary ComputeSummary(std::vector<ProbeSample> const& samples,
        std::vector<std::string> const& realmNames, int64 nowEpochMs);
    std::vector<TimelineIncident> BuildTimeline(std::vector<ProbeSample> const& samples,
        std::vector<PlannedWindow> const& planned, std::string const& operatorNote, int64 nowEpochMs);
    std::vector<RealmSparkline> BuildSparklines(std::vector<ProbeSample> const& samples,
        std::vector<std::string> const& realmNames, int64 nowEpochMs);
    nlohmann::json UptimeJson(UptimeSummary const& summary);
    nlohmann::json IncidentsJson(std::vector<TimelineIncident> const& timeline);
    nlohmann::json SparklinesJson(std::vector<RealmSparkline> const& sparklines);
    void AppendPublicFields(nlohmann::json& answer, UptimeSummary const& summary,
        std::vector<TimelineIncident> const& timeline, std::vector<RealmSparkline> const& sparklines);
    nlohmann::json OperatorAnswer(UptimeSummary const& summary,
        std::vector<TimelineIncident> const& timeline, std::vector<RealmSparkline> const& sparklines,
        int64 nowEpochMs);
    bool CheckShaped(nlohmann::json const& payload, std::string& offending);
    bool CheckOperatorShaped(nlohmann::json const& payload, std::string& offending);
}

#endif
