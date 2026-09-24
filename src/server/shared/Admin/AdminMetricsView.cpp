/*
 * Project Ambrose by Imjustchico
 * Writes the register out on request. The scrape is built from a snapshot of the entries so a metric registered while it is being written cannot change what is already in it, and it is sent as text rather than JSON because that is the only thing a scraper reads. The JSON answer groups the series that share a name under one entry, since that is how a reader thinks of them and how the panel draws them, and a histogram carries its bounds beside its counts so the panel needs to know nothing about the buckets this server chose.
 */

#include "AdminMetricsView.h"
#include "AdminRouter.h"
#include "MetricRegistry.h"
#include "StatsRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <variant>
#include <string>
#include <utility>
#include <vector>

namespace
{
    nlohmann::json LabelsJson(Ambrose::MetricLabels const& labels)
    {
        nlohmann::json out = nlohmann::json::object();
        for (auto const& [name, value] : labels)
            out[name] = value;
        return out;
    }

    char const* KindName(Ambrose::MetricKind kind) noexcept
    {
        switch (kind)
        {
            case Ambrose::MetricKind::Counter: return "counter";
            case Ambrose::MetricKind::Gauge: return "gauge";
            case Ambrose::MetricKind::Histogram: return "histogram";
        }
        return "untyped";
    }


    std::optional<double> AsNumber(Ambrose::StatValue const& value)
    {
        if (int64 const* const whole = std::get_if<int64>(&value))
            return static_cast<double>(*whole);
        if (double const* const real = std::get_if<double>(&value))
            return *real;
        if (bool const* const flag = std::get_if<bool>(&value))
            return *flag ? 1.0 : 0.0;
        return std::nullopt;
    }

    std::vector<Ambrose::MetricEntry> Everything()
    {
        std::vector<Ambrose::MetricEntry> entries = sMetrics.Collect();
        std::vector<std::string> taken;
        for (Ambrose::MetricEntry const& entry : entries)
            taken.push_back(entry.Name);
        for (auto const& [name, value] : sStats.Collect())
        {
            std::optional<double> const reading = AsNumber(value);
            if (!reading)
                continue;
            Ambrose::MetricEntry entry;
            entry.Name = std::string(Ambrose::MetricRegistry::Prefix) + name;
            entry.Help = "Published by the server as a live value";
            entry.Kind = Ambrose::MetricKind::Gauge;
            entry.Reading = *reading;
            if (std::find(taken.begin(), taken.end(), entry.Name) != taken.end())
                continue;
            entries.push_back(std::move(entry));
        }
        std::stable_sort(entries.begin(), entries.end(),
            [](Ambrose::MetricEntry const& left, Ambrose::MetricEntry const& right) { return left.Name < right.Name; });
        return entries;
    }

    nlohmann::json SeriesJson(Ambrose::MetricEntry const& metric)
    {
        nlohmann::json series;
        series["labels"] = LabelsJson(metric.Labels);
        if (metric.Reading)
            series["value"] = *metric.Reading;
        else if (metric.Kind == Ambrose::MetricKind::Counter && metric.AsCounter)
            series["value"] = metric.AsCounter->Value();
        else if (metric.Kind == Ambrose::MetricKind::Gauge && metric.AsGauge)
            series["value"] = metric.AsGauge->Value();
        else if (metric.Kind == Ambrose::MetricKind::Histogram && metric.AsHistogram)
        {
            series["count"] = metric.AsHistogram->Count();
            series["sum"] = metric.AsHistogram->Sum();
            nlohmann::json buckets = nlohmann::json::array();
            for (std::size_t bucket = 0; bucket < Ambrose::Histogram::BucketCount; ++bucket)
                buckets.push_back({ { "le", Ambrose::Histogram::Bounds[bucket] },
                    { "count", metric.AsHistogram->Cumulative(bucket) } });
            series["buckets"] = std::move(buckets);
        }
        return series;
    }
}

std::string AdminMetricsView::MetricsJson()
{
    std::vector<Ambrose::MetricEntry> const entries = Everything();
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    nlohmann::json metrics = nlohmann::json::array();
    std::string family;
    for (Ambrose::MetricEntry const& metric : entries)
    {
        if (!Ambrose::MetricRegistry::IsNameAllowed(metric.Name) || !Ambrose::MetricRegistry::AreLabelsAllowed(metric.Labels))
            continue;
        if (metrics.empty() || metric.Name != family)
        {
            nlohmann::json entry;
            entry["name"] = metric.Name;
            entry["help"] = metric.Help;
            entry["kind"] = KindName(metric.Kind);
            entry["series"] = nlohmann::json::array();
            metrics.push_back(std::move(entry));
            family = metric.Name;
        }
        metrics.back()["series"].push_back(SeriesJson(metric));
    }
    body["metrics"] = std::move(metrics);
    return body.dump();
}

void AdminMetricsView::Register(AdminRouter& router)
{
    router.AddGuarded("GET", "/metrics", "status.read", [](AdminRequest const&)
    {
        AdminResponse answer = AdminResponse::Json(200, Ambrose::MetricRegistry::Expose(Everything()));
        answer.ContentType = "text/plain; version=0.0.4; charset=utf-8";
        return answer;
    });

    router.AddGuarded("GET", "/api/metrics", "status.read", [](AdminRequest const&)
    {
        return AdminResponse::Json(200, MetricsJson());
    });
}
