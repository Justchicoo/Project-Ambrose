/*
 * Project Ambrose by Imjustchico
 * Reads an app's JSON metrics answer into names and numbers. A series that carries labels has them folded into its name, so one metric covering several pools or services becomes one history for each of them rather than one history that is the sum of things an operator wanted to tell apart. The ambrose prefix is dropped, since every series in this history belongs to this server and repeating it in every name would only make the panel's labels longer. Anything the answer does not describe as a counter, a gauge or a histogram is left alone rather than guessed at, and an answer that is not JSON at all is dropped without disturbing what was already recorded.
 */

#include "PublishedSampler.h"

#include <nlohmann/json.hpp>

#include <string_view>
#include <utility>

namespace
{
    constexpr std::string_view Prefix = "ambrose_";

    std::string NameOf(nlohmann::json const& metric, nlohmann::json const& series)
    {
        std::string name = metric.value("name", std::string());
        if (name.starts_with(Prefix))
            name.erase(0, Prefix.size());
        auto const labels = series.find("labels");
        if (labels == series.end() || !labels->is_object())
            return name;
        for (auto const& [label, value] : labels->items())
        {
            if (!value.is_string())
                continue;
            name += '_';
            name += value.get<std::string>();
        }
        return name;
    }
}

PublishedSampler::PublishedSampler(Ambrose::SeriesStore& store, Reader reader) : _store(store), _reader(std::move(reader))
{
}

void PublishedSampler::Forget(std::string const& app)
{
    _last.erase(app);
}

std::map<std::string, double> PublishedSampler::Read(std::string const& json, Totals& totals)
{
    std::map<std::string, double> gauges;
    nlohmann::json const body = nlohmann::json::parse(json, nullptr, false);
    if (!body.is_object())
        return gauges;
    auto const metrics = body.find("metrics");
    if (metrics == body.end() || !metrics->is_array())
        return gauges;

    for (nlohmann::json const& metric : *metrics)
    {
        std::string const kind = metric.value("kind", std::string());
        auto const series = metric.find("series");
        if (series == metric.end() || !series->is_array())
            continue;
        for (nlohmann::json const& one : *series)
        {
            std::string const name = NameOf(metric, one);
            if (name.empty())
                continue;
            if (kind == "gauge")
            {
                if (one.contains("value") && one["value"].is_number())
                    gauges[name] = one["value"].get<double>();
            }
            else if (kind == "counter")
            {
                if (one.contains("value") && one["value"].is_number())
                    totals[name] = { false, one["value"].get<double>(), 0.0 };
            }
            else if (kind == "histogram")
            {
                if (one.contains("sum") && one["sum"].is_number() && one.contains("count") && one["count"].is_number())
                    totals[name] = { true, one["sum"].get<double>(), one["count"].get<double>() };
            }
        }
    }
    return gauges;
}

std::vector<std::string> PublishedSampler::Sample(std::vector<AppSnapshot> const& apps, int64 atMilliseconds)
{
    std::vector<std::string> read;
    for (AppSnapshot const& app : apps)
    {
        if (!app.ProcessId || *app.ProcessId <= 0)
        {
            Forget(app.Name);
            continue;
        }
        std::optional<std::string> const answer = _reader(app.Name);
        if (!answer)
        {
            Forget(app.Name);
            continue;
        }

        Totals totals;
        std::map<std::string, double> const gauges = Read(*answer, totals);
        if (gauges.empty() && totals.empty())
            continue;

        for (auto const& [name, value] : gauges)
            _store.Add(app.Name, name, atMilliseconds, value);

        Last& last = _last[app.Name];
        if (last.Seen && atMilliseconds > last.AtMilliseconds)
        {
            double const seconds = static_cast<double>(atMilliseconds - last.AtMilliseconds) / 1000.0;
            for (auto const& [name, now] : totals)
            {
                auto const before = last.Amounts.find(name);
                if (before == last.Amounts.end() || before->second.Histogram != now.Histogram)
                    continue;
                double const sum = now.Sum - before->second.Sum;
                double const count = now.Count - before->second.Count;
                if (sum < 0.0 || count < 0.0)
                    continue;
                if (!now.Histogram)
                    _store.Add(app.Name, name + "_per_second", atMilliseconds, sum / seconds);
                else if (count > 0.0)
                    _store.Add(app.Name, name + "_mean", atMilliseconds, sum / count);
            }
        }
        last.Seen = true;
        last.AtMilliseconds = atMilliseconds;
        last.Amounts = std::move(totals);
        read.push_back(app.Name);
    }
    return read;
}
