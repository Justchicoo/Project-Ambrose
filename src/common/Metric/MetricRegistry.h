/*
 * Project Ambrose by Imjustchico
 * Where a subsystem registers what it counts and where a scrape reads it: each metric is named once, given the line of help a reader needs and kept for the life of the process, so a handler takes a reference at startup and pays nothing for the lookup afterwards. A metric already registered under a name and a set of labels is handed back rather than made twice, because two subsystems counting the same thing under one name would each see half of it. Labels are what lets one name cover every target, service or pool and still be summed or split apart by a reader, and they are sorted into one canonical order so the same series asked for two ways is one series. Names are checked against what the Prometheus text format allows, since a name it refuses would make the whole scrape unreadable rather than just that line. An entry may carry a reading of its own instead of pointing at a metric, which is how a value another register already works out at read time is written into the same scrape rather than counted a second time here. Clearing the register puts what it held aside instead of destroying it, because a subsystem holds the reference it was given for as long as it runs, and a test that wanted a clean slate would otherwise leave that reference pointing at nothing.
 */

#ifndef AMBROSE_METRICREGISTRY_H
#define AMBROSE_METRICREGISTRY_H

#include "Metric.h"

#include <map>
#include <memory>
#include <optional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ambrose
{
    enum class MetricKind : uint8
    {
        Counter,
        Gauge,
        Histogram
    };

    using MetricLabels = std::vector<std::pair<std::string, std::string>>;

    struct MetricEntry
    {
        std::string Name;
        std::string Help;
        MetricLabels Labels;
        MetricKind Kind = MetricKind::Counter;
        Counter* AsCounter = nullptr;
        Gauge* AsGauge = nullptr;
        Histogram* AsHistogram = nullptr;
        std::optional<double> Reading;
    };

    class MetricRegistry
    {
    public:
        static constexpr std::string_view Prefix = "ambrose_";

        static MetricRegistry& Instance();

        MetricRegistry(MetricRegistry const&) = delete;
        MetricRegistry& operator=(MetricRegistry const&) = delete;

        Counter& CounterFor(std::string name, std::string help, MetricLabels labels = {});
        Gauge& GaugeFor(std::string name, std::string help, MetricLabels labels = {});
        Histogram& HistogramFor(std::string name, std::string help, MetricLabels labels = {});

        std::vector<MetricEntry> Collect() const;
        std::size_t Count() const;
        void Clear();

        static bool IsNameAllowed(std::string_view name) noexcept;
        static bool AreLabelsAllowed(MetricLabels const& labels) noexcept;
        static std::string Expose(std::vector<MetricEntry> metrics);

    private:
        MetricRegistry() = default;

        struct Held
        {
            MetricEntry Entry;
            std::unique_ptr<Counter> OwnedCounter;
            std::unique_ptr<Gauge> OwnedGauge;
            std::unique_ptr<Histogram> OwnedHistogram;
        };

        static std::string SeriesKey(std::string_view name, MetricLabels const& labels);

        Held& Take(std::string name, std::string help, MetricLabels labels, MetricKind kind);

        mutable std::mutex _mutex;
        std::map<std::string, Held, std::less<>> _metrics;
        std::vector<Held> _retired;
    };
}

#define sMetrics Ambrose::MetricRegistry::Instance()

#endif
