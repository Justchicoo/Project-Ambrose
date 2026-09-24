/*
 * Project Ambrose by Imjustchico
 * Hands out the metric behind a name and a set of labels, making it the first time and returning the same one every time after, and writes the whole set in the Prometheus text exposition format. A name or a label the format would refuse is refused here instead, where it is one caller's mistake rather than an unreadable scrape. Help and label values are escaped because a stray newline or quote in one would end the metric early and take everything after it with it, and a histogram is written with its cumulative buckets, its sum and its count, which is the shape a reader expects and the only shape it can compute a quantile from. The help and type lines are written once for a name however many label sets it carries, since a reader takes a second pair as a duplicate metric and rejects the scrape.
 */

#include "MetricRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
    std::string Escape(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char const letter : text)
        {
            if (letter == '\\')
                escaped += "\\\\";
            else if (letter == '\n')
                escaped += "\\n";
            else if (letter == '"')
                escaped += "\\\"";
            else
                escaped += letter;
        }
        return escaped;
    }

    std::string EscapeHelp(std::string_view help)
    {
        std::string escaped;
        escaped.reserve(help.size());
        for (char const letter : help)
        {
            if (letter == '\\')
                escaped += "\\\\";
            else if (letter == '\n')
                escaped += "\\n";
            else
                escaped += letter;
        }
        return escaped;
    }

    std::string_view KindName(Ambrose::MetricKind kind) noexcept
    {
        switch (kind)
        {
            case Ambrose::MetricKind::Counter: return "counter";
            case Ambrose::MetricKind::Gauge: return "gauge";
            case Ambrose::MetricKind::Histogram: return "histogram";
        }
        return "untyped";
    }

    std::string Number(double value)
    {
        return fmt::format("{}", value);
    }

    std::string LabelText(Ambrose::MetricLabels const& labels, std::string_view extraName = {}, std::string_view extraValue = {})
    {
        if (labels.empty() && extraName.empty())
            return {};
        std::string text = "{";
        for (auto const& [name, value] : labels)
        {
            if (text.size() > 1)
                text += ',';
            text += fmt::format("{}=\"{}\"", name, Escape(value));
        }
        if (!extraName.empty())
        {
            if (text.size() > 1)
                text += ',';
            text += fmt::format("{}=\"{}\"", extraName, extraValue);
        }
        text += '}';
        return text;
    }
}

namespace Ambrose
{
    MetricRegistry& MetricRegistry::Instance()
    {
        static MetricRegistry instance;
        return instance;
    }

    bool MetricRegistry::IsNameAllowed(std::string_view name) noexcept
    {
        if (name.empty())
            return false;
        char const first = name.front();
        if (!(std::isalpha(static_cast<unsigned char>(first)) || first == '_' || first == ':'))
            return false;
        return std::all_of(name.begin(), name.end(), [](char const letter)
        {
            return std::isalnum(static_cast<unsigned char>(letter)) || letter == '_' || letter == ':';
        });
    }

    bool MetricRegistry::AreLabelsAllowed(MetricLabels const& labels) noexcept
    {
        for (auto const& [name, value] : labels)
        {
            if (name.empty() || name == "le" || name.starts_with("__"))
                return false;
            char const first = name.front();
            if (!(std::isalpha(static_cast<unsigned char>(first)) || first == '_'))
                return false;
            if (!std::all_of(name.begin(), name.end(), [](char const letter)
                { return std::isalnum(static_cast<unsigned char>(letter)) || letter == '_'; }))
                return false;
        }
        return true;
    }

    std::string MetricRegistry::SeriesKey(std::string_view name, MetricLabels const& labels)
    {
        std::string key(name);
        for (auto const& [label, value] : labels)
        {
            key += '\x1f';
            key += label;
            key += '\x1e';
            key += value;
        }
        return key;
    }

    MetricRegistry::Held& MetricRegistry::Take(std::string name, std::string help, MetricLabels labels, MetricKind kind)
    {
        std::sort(labels.begin(), labels.end());
        std::string key = SeriesKey(name, labels);
        auto const found = _metrics.find(key);
        if (found != _metrics.end())
            return found->second;

        Held held;
        held.Entry.Name = std::move(name);
        held.Entry.Help = std::move(help);
        held.Entry.Labels = std::move(labels);
        held.Entry.Kind = kind;
        switch (kind)
        {
            case MetricKind::Counter:
                held.OwnedCounter = std::make_unique<Counter>();
                held.Entry.AsCounter = held.OwnedCounter.get();
                break;
            case MetricKind::Gauge:
                held.OwnedGauge = std::make_unique<Gauge>();
                held.Entry.AsGauge = held.OwnedGauge.get();
                break;
            case MetricKind::Histogram:
                held.OwnedHistogram = std::make_unique<Histogram>();
                held.Entry.AsHistogram = held.OwnedHistogram.get();
                break;
        }
        return _metrics.emplace(std::move(key), std::move(held)).first->second;
    }

    Counter& MetricRegistry::CounterFor(std::string name, std::string help, MetricLabels labels)
    {
        std::lock_guard const lock(_mutex);
        Held& held = Take(std::move(name), std::move(help), std::move(labels), MetricKind::Counter);
        static Counter refused;
        return held.Entry.AsCounter ? *held.Entry.AsCounter : refused;
    }

    Gauge& MetricRegistry::GaugeFor(std::string name, std::string help, MetricLabels labels)
    {
        std::lock_guard const lock(_mutex);
        Held& held = Take(std::move(name), std::move(help), std::move(labels), MetricKind::Gauge);
        static Gauge refused;
        return held.Entry.AsGauge ? *held.Entry.AsGauge : refused;
    }

    Histogram& MetricRegistry::HistogramFor(std::string name, std::string help, MetricLabels labels)
    {
        std::lock_guard const lock(_mutex);
        Held& held = Take(std::move(name), std::move(help), std::move(labels), MetricKind::Histogram);
        static Histogram refused;
        return held.Entry.AsHistogram ? *held.Entry.AsHistogram : refused;
    }

    std::vector<MetricEntry> MetricRegistry::Collect() const
    {
        std::lock_guard const lock(_mutex);
        std::vector<MetricEntry> entries;
        entries.reserve(_metrics.size());
        for (auto const& [key, held] : _metrics)
            entries.push_back(held.Entry);
        return entries;
    }

    std::size_t MetricRegistry::Count() const
    {
        std::lock_guard const lock(_mutex);
        return _metrics.size();
    }

    void MetricRegistry::Clear()
    {
        std::lock_guard const lock(_mutex);
        for (auto& [key, held] : _metrics)
            _retired.push_back(std::move(held));
        _metrics.clear();
    }

    std::string MetricRegistry::Expose(std::vector<MetricEntry> metrics)
    {
        std::erase_if(metrics, [](MetricEntry const& metric)
            { return !IsNameAllowed(metric.Name) || !AreLabelsAllowed(metric.Labels); });
        std::stable_sort(metrics.begin(), metrics.end(),
            [](MetricEntry const& left, MetricEntry const& right) { return left.Name < right.Name; });

        std::string text;
        std::string_view family;
        bool first = true;
        for (MetricEntry const& metric : metrics)
        {
            if (first || metric.Name != family)
            {
                if (!metric.Help.empty())
                    text += fmt::format("# HELP {} {}\n", metric.Name, EscapeHelp(metric.Help));
                text += fmt::format("# TYPE {} {}\n", metric.Name, KindName(metric.Kind));
                family = metric.Name;
                first = false;
            }

            std::string const labels = LabelText(metric.Labels);
            if (metric.Reading)
                text += fmt::format("{}{} {}\n", metric.Name, labels, Number(*metric.Reading));
            else if (metric.Kind == MetricKind::Counter && metric.AsCounter)
                text += fmt::format("{}{} {}\n", metric.Name, labels, metric.AsCounter->Value());
            else if (metric.Kind == MetricKind::Gauge && metric.AsGauge)
                text += fmt::format("{}{} {}\n", metric.Name, labels, metric.AsGauge->Value());
            else if (metric.Kind == MetricKind::Histogram && metric.AsHistogram)
            {
                for (std::size_t bucket = 0; bucket < Histogram::BucketCount; ++bucket)
                    text += fmt::format("{}_bucket{} {}\n", metric.Name,
                        LabelText(metric.Labels, "le", Number(Histogram::Bounds[bucket])),
                        metric.AsHistogram->Cumulative(bucket));
                text += fmt::format("{}_bucket{} {}\n", metric.Name, LabelText(metric.Labels, "le", "+Inf"),
                    metric.AsHistogram->Count());
                text += fmt::format("{}_sum{} {}\n", metric.Name, labels, Number(metric.AsHistogram->Sum()));
                text += fmt::format("{}_count{} {}\n", metric.Name, labels, metric.AsHistogram->Count());
            }
        }
        return text;
    }
}
