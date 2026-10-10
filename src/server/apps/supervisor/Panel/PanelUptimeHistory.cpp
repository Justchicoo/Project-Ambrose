/*
 * Project Ambrose by Imjustchico
 * Uptime history and incident timeline 17.94: per-realm and login-path uptime percentages over a day, a month and a year computed from the probe's own samples, the incident timeline with planned maintenance windows marked as planned, per-realm sparklines, and the aggregate-only shaping of the extended public and operator payloads.
 */

#include "PanelUptimeHistory.h"

#include "PanelPublicStatus.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>

namespace
{
    UptimeFigure FigureFor(std::vector<ProbeSample> const& samples, std::string const& target,
        int64 fromEpochMs, int64 nowEpochMs)
    {
        int64 ok = 0;
        int64 total = 0;
        for (ProbeSample const& sample : samples)
        {
            if (sample.Target != target || sample.EpochMs < fromEpochMs || sample.EpochMs > nowEpochMs)
                continue;
            ++total;
            if (sample.Ok)
                ++ok;
        }
        UptimeFigure figure;
        figure.Known = total > 0;
        if (figure.Known)
            figure.Percent = 100.0 * static_cast<double>(ok) / static_cast<double>(total);
        return figure;
    }

    UptimeWindows WindowsFor(std::vector<ProbeSample> const& samples, std::string const& target, int64 nowEpochMs)
    {
        UptimeWindows windows;
        windows.Day = FigureFor(samples, target, nowEpochMs - PanelUptimeHistory::MsPerDay, nowEpochMs);
        windows.Month = FigureFor(samples, target, nowEpochMs - PanelUptimeHistory::MsPerMonth, nowEpochMs);
        windows.Year = FigureFor(samples, target, nowEpochMs - PanelUptimeHistory::MsPerYear, nowEpochMs);
        return windows;
    }

    bool Overlaps(int64 firstStart, int64 firstEnd, int64 secondStart, int64 secondEnd)
    {
        return firstStart <= secondEnd && secondStart <= firstEnd;
    }

    nlohmann::json FigureJson(UptimeFigure const& figure)
    {
        return figure.Known ? nlohmann::json(figure.Percent) : nlohmann::json();
    }

    nlohmann::json WindowsJson(UptimeWindows const& windows)
    {
        return {
            { "day", FigureJson(windows.Day) },
            { "month", FigureJson(windows.Month) },
            { "year", FigureJson(windows.Year) },
        };
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

    bool CheckWindows(nlohmann::json const& value, std::string& offending)
    {
        if (!ShapedObject(value, { "day", "month", "year" }, offending))
            return false;
        for (char const* key : { "day", "month", "year" })
        {
            nlohmann::json const& figure = value[key];
            if (figure.is_null())
                continue;
            if (!figure.is_number() || figure.get<double>() < 0.0 || figure.get<double>() > 100.0)
            {
                offending = key;
                return false;
            }
        }
        return true;
    }

    bool CheckUptime(nlohmann::json const& value, std::string& offending)
    {
        if (!ShapedObject(value, { "computed_epoch_ms", "realms", "login" }, offending))
            return false;
        if (!value["computed_epoch_ms"].is_number_integer())
        {
            offending = "uptime.computed_epoch_ms";
            return false;
        }
        if (!value["realms"].is_object())
        {
            offending = "uptime.realms";
            return false;
        }
        for (auto const& [name, windows] : value["realms"].items())
        {
            if (!CheckWindows(windows, offending))
            {
                offending = "uptime.realms." + name + "." + offending;
                return false;
            }
        }
        if (!CheckWindows(value["login"], offending))
        {
            offending = "uptime.login." + offending;
            return false;
        }
        return true;
    }

    bool CheckIncidents(nlohmann::json const& value, std::string& offending)
    {
        if (!value.is_array())
        {
            offending = "incidents";
            return false;
        }
        for (nlohmann::json const& incident : value)
        {
            if (!ShapedObject(incident, { "start_epoch_ms", "end_epoch_ms", "cause", "planned", "note" }, offending))
                return false;
            if (!incident["start_epoch_ms"].is_number_integer() || !incident["end_epoch_ms"].is_number_integer())
            {
                offending = "incident.epoch";
                return false;
            }
            if (!incident["cause"].is_string() || !incident["note"].is_string() || !incident["planned"].is_boolean())
            {
                offending = "incident.field";
                return false;
            }
        }
        return true;
    }

    bool CheckSparklines(nlohmann::json const& value, std::string& offending)
    {
        if (!value.is_object())
        {
            offending = "sparklines";
            return false;
        }
        for (auto const& [name, buckets] : value.items())
        {
            if (!buckets.is_array())
            {
                offending = "sparklines." + name;
                return false;
            }
            for (nlohmann::json const& bucket : buckets)
            {
                if (bucket.is_null())
                    continue;
                if (!bucket.is_number() || bucket.get<double>() < 0.0 || bucket.get<double>() > 1.0)
                {
                    offending = "sparklines." + name + ".bucket";
                    return false;
                }
            }
        }
        return true;
    }
}

namespace PanelUptimeHistory
{
    UptimeSummary ComputeSummary(std::vector<ProbeSample> const& samples,
        std::vector<std::string> const& realmNames, int64 nowEpochMs)
    {
        UptimeSummary summary;
        summary.ComputedEpochMs = nowEpochMs;
        for (std::string const& name : realmNames)
            summary.Realms[name] = WindowsFor(samples, name, nowEpochMs);
        summary.Login = WindowsFor(samples, std::string(LoginTarget), nowEpochMs);
        return summary;
    }

    std::vector<TimelineIncident> BuildTimeline(std::vector<ProbeSample> const& samples,
        std::vector<PlannedWindow> const& planned, std::string const& operatorNote, int64 nowEpochMs)
    {
        std::vector<TimelineIncident> timeline;
        for (PlannedWindow const& window : planned)
        {
            TimelineIncident entry;
            entry.StartEpochMs = window.StartEpochMs;
            entry.EndEpochMs = window.EndEpochMs;
            entry.Cause = window.Reason;
            entry.Planned = true;
            timeline.push_back(std::move(entry));
        }
        std::map<std::string, std::vector<ProbeSample>> byTarget;
        for (ProbeSample const& sample : samples)
            byTarget[sample.Target].push_back(sample);
        for (auto& [target, series] : byTarget)
        {
            std::sort(series.begin(), series.end(),
                [](ProbeSample const& first, ProbeSample const& second) { return first.EpochMs < second.EpochMs; });
            std::size_t begin = 0;
            while (begin < series.size())
            {
                if (series[begin].Ok)
                {
                    ++begin;
                    continue;
                }
                std::size_t end = begin;
                while (end + 1 < series.size() && !series[end + 1].Ok)
                    ++end;
                int64 startEpochMs = series[begin].EpochMs;
                int64 endEpochMs = (end + 1 == series.size()) ? nowEpochMs : series[end].EpochMs;
                bool covered = false;
                for (PlannedWindow const& window : planned)
                {
                    if (Overlaps(startEpochMs, endEpochMs, window.StartEpochMs, window.EndEpochMs))
                    {
                        covered = true;
                        break;
                    }
                }
                if (!covered)
                {
                    TimelineIncident incident;
                    incident.StartEpochMs = startEpochMs;
                    incident.EndEpochMs = endEpochMs;
                    incident.Cause = std::string("The probe could not reach ")
                        + (target == LoginTarget ? "the login path" : target);
                    timeline.push_back(std::move(incident));
                }
                begin = end + 1;
            }
        }
        if (!operatorNote.empty() && !timeline.empty())
        {
            auto latest = std::max_element(timeline.begin(), timeline.end(),
                [](TimelineIncident const& first, TimelineIncident const& second)
                { return first.StartEpochMs < second.StartEpochMs; });
            latest->Note = operatorNote;
        }
        std::sort(timeline.begin(), timeline.end(),
            [](TimelineIncident const& first, TimelineIncident const& second)
            { return first.StartEpochMs > second.StartEpochMs; });
        return timeline;
    }

    std::vector<RealmSparkline> BuildSparklines(std::vector<ProbeSample> const& samples,
        std::vector<std::string> const& realmNames, int64 nowEpochMs)
    {
        std::vector<RealmSparkline> sparklines;
        for (std::string const& name : realmNames)
        {
            RealmSparkline sparkline;
            sparkline.Realm = name;
            sparkline.Buckets.resize(SparklineBuckets);
            for (int bucket = 0; bucket < SparklineBuckets; ++bucket)
            {
                int64 bucketEnd = nowEpochMs - (SparklineBuckets - 1 - bucket) * MsPerHour;
                int64 bucketStart = bucketEnd - MsPerHour;
                int64 ok = 0;
                int64 total = 0;
                for (ProbeSample const& sample : samples)
                {
                    if (sample.Target != name || sample.EpochMs <= bucketStart || sample.EpochMs > bucketEnd)
                        continue;
                    ++total;
                    if (sample.Ok)
                        ++ok;
                }
                if (total > 0)
                    sparkline.Buckets[bucket] = static_cast<double>(ok) / static_cast<double>(total);
            }
            sparklines.push_back(std::move(sparkline));
        }
        return sparklines;
    }

    nlohmann::json UptimeJson(UptimeSummary const& summary)
    {
        nlohmann::json realms = nlohmann::json::object();
        for (auto const& [name, windows] : summary.Realms)
            realms[name] = WindowsJson(windows);
        return {
            { "computed_epoch_ms", summary.ComputedEpochMs },
            { "realms", std::move(realms) },
            { "login", WindowsJson(summary.Login) },
        };
    }

    nlohmann::json IncidentsJson(std::vector<TimelineIncident> const& timeline)
    {
        nlohmann::json incidents = nlohmann::json::array();
        for (TimelineIncident const& incident : timeline)
        {
            incidents.push_back({
                { "start_epoch_ms", incident.StartEpochMs },
                { "end_epoch_ms", incident.EndEpochMs },
                { "cause", incident.Cause },
                { "planned", incident.Planned },
                { "note", incident.Note },
            });
        }
        return incidents;
    }

    nlohmann::json SparklinesJson(std::vector<RealmSparkline> const& sparklines)
    {
        nlohmann::json shaped = nlohmann::json::object();
        for (RealmSparkline const& sparkline : sparklines)
        {
            nlohmann::json buckets = nlohmann::json::array();
            for (std::optional<double> const& bucket : sparkline.Buckets)
                buckets.push_back(bucket ? nlohmann::json(*bucket) : nlohmann::json());
            shaped[sparkline.Realm] = std::move(buckets);
        }
        return shaped;
    }

    void AppendPublicFields(nlohmann::json& answer, UptimeSummary const& summary,
        std::vector<TimelineIncident> const& timeline, std::vector<RealmSparkline> const& sparklines)
    {
        answer["uptime"] = UptimeJson(summary);
        answer["incidents"] = IncidentsJson(timeline);
        answer["sparklines"] = SparklinesJson(sparklines);
    }

    nlohmann::json OperatorAnswer(UptimeSummary const& summary,
        std::vector<TimelineIncident> const& timeline, std::vector<RealmSparkline> const& sparklines,
        int64 nowEpochMs)
    {
        return {
            { "generated_epoch_ms", nowEpochMs },
            { "uptime", UptimeJson(summary) },
            { "incidents", IncidentsJson(timeline) },
            { "sparklines", SparklinesJson(sparklines) },
        };
    }

    bool CheckShaped(nlohmann::json const& payload, std::string& offending)
    {
        offending.clear();
        if (!ShapedObject(payload,
            { "realms", "login_accepting_players", "maintenance", "incident", "generated_epoch_ms",
              "uptime", "incidents", "sparklines" }, offending))
            return false;
        nlohmann::json base = payload;
        base.erase("uptime");
        base.erase("incidents");
        base.erase("sparklines");
        if (!PanelPublicStatus::CheckShaped(base, offending))
            return false;
        if (!CheckUptime(payload["uptime"], offending))
            return false;
        if (!CheckIncidents(payload["incidents"], offending))
            return false;
        if (!CheckSparklines(payload["sparklines"], offending))
            return false;
        return true;
    }

    bool CheckOperatorShaped(nlohmann::json const& payload, std::string& offending)
    {
        offending.clear();
        if (!ShapedObject(payload, { "generated_epoch_ms", "uptime", "incidents", "sparklines" }, offending))
            return false;
        if (!payload["generated_epoch_ms"].is_number_integer())
        {
            offending = "generated_epoch_ms";
            return false;
        }
        if (!CheckUptime(payload["uptime"], offending))
            return false;
        if (!CheckIncidents(payload["incidents"], offending))
            return false;
        if (!CheckSparklines(payload["sparklines"], offending))
            return false;
        return true;
    }
}
