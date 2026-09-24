/*
 * Project Ambrose by Imjustchico
 * The history of one number over time, kept at more than one resolution so that asking for five minutes and asking for thirty days both read a bounded number of points. A sample is folded into every resolution as it arrives rather than by sweeping the fine one later, because a supervisor that was stopped when the sweep was due would otherwise lose the coarse point entirely. Each resolution is a ring of buckets that remembers which bucket it holds, so a slot the ring has wrapped past reads as absent rather than as an old value in a new place. A bucket nothing was written into is absent and not zero, because an app that was stopped drew no samples and a zero would read as an app that was running and idle, which is the opposite of what happened. A bucket carries the mean, the lowest and the highest of what went into it, so a spike between two reads is still visible after it has been folded down.
 */

#ifndef AMBROSE_TIMESERIES_H
#define AMBROSE_TIMESERIES_H

#include "Types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Ambrose
{
    struct SeriesPoint
    {
        int64 AtMilliseconds = 0;
        bool Present = false;
        double Mean = 0.0;
        double Lowest = 0.0;
        double Highest = 0.0;
        uint32 Samples = 0;
    };

    struct SeriesBucket
    {
        int64 Bucket = 0;
        double Sum = 0.0;
        double Lowest = 0.0;
        double Highest = 0.0;
        uint32 Samples = 0;
    };

    struct SeriesResolution
    {
        int64 BucketMilliseconds = 0;
        std::size_t Buckets = 0;

        int64 Span() const noexcept { return BucketMilliseconds * static_cast<int64>(Buckets); }
    };

    class SeriesTier
    {
    public:
        SeriesTier(int64 bucketMilliseconds, std::size_t buckets);

        bool Add(int64 atMilliseconds, double value);
        std::vector<SeriesBucket> Export() const;
        void Restore(std::vector<SeriesBucket> const& buckets);
        std::vector<SeriesPoint> Between(int64 fromMilliseconds, int64 toMilliseconds) const;

        int64 BucketMilliseconds() const noexcept { return _bucketMilliseconds; }
        std::size_t Buckets() const noexcept { return _slots.size(); }
        int64 Span() const noexcept { return _bucketMilliseconds * static_cast<int64>(_slots.size()); }
        int64 Newest() const noexcept { return _newestBucket; }
        std::size_t Held() const;

    private:
        struct Slot
        {
            int64 Bucket = -1;
            double Sum = 0.0;
            double Lowest = 0.0;
            double Highest = 0.0;
            uint32 Samples = 0;
        };

        int64 BucketOf(int64 atMilliseconds) const noexcept;
        std::size_t IndexOf(int64 bucket) const noexcept;

        int64 _bucketMilliseconds;
        int64 _newestBucket = -1;
        std::vector<Slot> _slots;
    };

    class TimeSeries
    {
    public:
        static std::vector<SeriesResolution> const& DefaultResolutions();

        TimeSeries();
        explicit TimeSeries(std::vector<SeriesResolution> const& resolutions);

        void Add(int64 atMilliseconds, double value);
        std::vector<SeriesBucket> ExportCoarsest() const;
        void RestoreCoarsest(std::vector<SeriesBucket> const& buckets);
        std::vector<SeriesPoint> Between(int64 fromMilliseconds, int64 toMilliseconds, std::size_t mostPoints) const;

        std::vector<SeriesTier> const& Tiers() const noexcept { return _tiers; }
        int64 CoarsestBucketMilliseconds() const noexcept;
        int64 FinestBucketMilliseconds() const noexcept;

    private:
        static std::vector<SeriesPoint> Group(std::vector<SeriesPoint> const& points, std::size_t mostPoints, int64 bucketMilliseconds);

        std::vector<SeriesTier> _tiers;
    };
}

#endif
