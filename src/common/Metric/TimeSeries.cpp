/*
 * Project Ambrose by Imjustchico
 * Folds a sample into every resolution as it arrives and reads a range back from the finest resolution that still covers it. A sample older than a ring can hold is refused rather than written into a slot that belongs to a newer bucket, since the ring is indexed by bucket and an out-of-range write would silently corrupt the point already there. Each resolution reaches back only as far as a reader can ask for at that resolution, since a range comes back as a bounded number of points and nothing can request a day of five-second samples. Reading picks the finest resolution whose oldest bucket is still old enough to answer the whole range and that actually holds something in it, so a short range keeps its detail, a long one is not assembled from points that were dropped days ago, and a short range asked for just after a restart is answered from the coarse resolution that was saved rather than from the fine one that only lives in memory and is therefore empty, and if even the coarsest resolution holds more buckets than the caller will take, adjacent buckets are grouped until it does, keeping the lowest and highest across the group rather than the mean of the means alone.
 */

#include "TimeSeries.h"

#include <algorithm>
#include <cmath>

namespace Ambrose
{
    SeriesTier::SeriesTier(int64 bucketMilliseconds, std::size_t buckets)
        : _bucketMilliseconds(bucketMilliseconds < 1 ? 1 : bucketMilliseconds), _slots(buckets < 1 ? 1 : buckets)
    {
    }

    int64 SeriesTier::BucketOf(int64 atMilliseconds) const noexcept
    {
        int64 const floored = atMilliseconds >= 0
            ? atMilliseconds / _bucketMilliseconds
            : -((-atMilliseconds + _bucketMilliseconds - 1) / _bucketMilliseconds);
        return floored;
    }

    std::size_t SeriesTier::IndexOf(int64 bucket) const noexcept
    {
        int64 const count = static_cast<int64>(_slots.size());
        int64 const index = ((bucket % count) + count) % count;
        return static_cast<std::size_t>(index);
    }

    bool SeriesTier::Add(int64 atMilliseconds, double value)
    {
        if (!std::isfinite(value))
            return false;
        int64 const bucket = BucketOf(atMilliseconds);
        if (_newestBucket >= 0 && bucket <= _newestBucket - static_cast<int64>(_slots.size()))
            return false;

        Slot& slot = _slots[IndexOf(bucket)];
        if (slot.Bucket != bucket)
        {
            slot.Bucket = bucket;
            slot.Sum = 0.0;
            slot.Lowest = value;
            slot.Highest = value;
            slot.Samples = 0;
        }
        slot.Sum += value;
        slot.Lowest = std::min(slot.Lowest, value);
        slot.Highest = std::max(slot.Highest, value);
        ++slot.Samples;
        _newestBucket = std::max(_newestBucket, bucket);
        return true;
    }

    std::vector<SeriesBucket> SeriesTier::Export() const
    {
        std::vector<SeriesBucket> buckets;
        for (Slot const& slot : _slots)
        {
            if (slot.Bucket < 0 || slot.Samples == 0)
                continue;
            SeriesBucket made;
            made.Bucket = slot.Bucket;
            made.Sum = slot.Sum;
            made.Lowest = slot.Lowest;
            made.Highest = slot.Highest;
            made.Samples = slot.Samples;
            buckets.push_back(made);
        }
        std::sort(buckets.begin(), buckets.end(),
            [](SeriesBucket const& left, SeriesBucket const& right) { return left.Bucket < right.Bucket; });
        return buckets;
    }

    void SeriesTier::Restore(std::vector<SeriesBucket> const& buckets)
    {
        for (SeriesBucket const& bucket : buckets)
        {
            if (bucket.Samples == 0 || bucket.Bucket < 0)
                continue;
            if (_newestBucket >= 0 && bucket.Bucket <= _newestBucket - static_cast<int64>(_slots.size()))
                continue;
            Slot& slot = _slots[IndexOf(bucket.Bucket)];
            if (slot.Bucket == bucket.Bucket && slot.Samples >= bucket.Samples)
                continue;
            slot.Bucket = bucket.Bucket;
            slot.Sum = bucket.Sum;
            slot.Lowest = bucket.Lowest;
            slot.Highest = bucket.Highest;
            slot.Samples = bucket.Samples;
            _newestBucket = std::max(_newestBucket, bucket.Bucket);
        }
    }

    std::size_t SeriesTier::Held() const
    {
        std::size_t held = 0;
        for (Slot const& slot : _slots)
            if (slot.Bucket >= 0 && slot.Samples > 0)
                ++held;
        return held;
    }

    std::vector<SeriesPoint> SeriesTier::Between(int64 fromMilliseconds, int64 toMilliseconds) const
    {
        std::vector<SeriesPoint> points;
        if (toMilliseconds < fromMilliseconds)
            return points;
        int64 const first = BucketOf(fromMilliseconds);
        int64 const last = BucketOf(toMilliseconds);
        if (last - first + 1 > static_cast<int64>(_slots.size()) * 4)
            return points;

        points.reserve(static_cast<std::size_t>(last - first + 1));
        for (int64 bucket = first; bucket <= last; ++bucket)
        {
            SeriesPoint point;
            point.AtMilliseconds = bucket * _bucketMilliseconds;
            Slot const& slot = _slots[IndexOf(bucket)];
            if (slot.Bucket == bucket && slot.Samples > 0)
            {
                point.Present = true;
                point.Mean = slot.Sum / slot.Samples;
                point.Lowest = slot.Lowest;
                point.Highest = slot.Highest;
                point.Samples = slot.Samples;
            }
            points.push_back(point);
        }
        return points;
    }

    std::vector<SeriesResolution> const& TimeSeries::DefaultResolutions()
    {
        static std::vector<SeriesResolution> const resolutions{
            { 5000, 720 },
            { 60000, 1440 },
            { 900000, 2880 },
        };
        return resolutions;
    }

    TimeSeries::TimeSeries() : TimeSeries(DefaultResolutions())
    {
    }

    TimeSeries::TimeSeries(std::vector<SeriesResolution> const& resolutions)
    {
        _tiers.reserve(resolutions.size());
        for (SeriesResolution const& resolution : resolutions)
            _tiers.emplace_back(resolution.BucketMilliseconds, resolution.Buckets);
        std::stable_sort(_tiers.begin(), _tiers.end(),
            [](SeriesTier const& left, SeriesTier const& right) { return left.BucketMilliseconds() < right.BucketMilliseconds(); });
    }

    int64 TimeSeries::FinestBucketMilliseconds() const noexcept
    {
        return _tiers.empty() ? 0 : _tiers.front().BucketMilliseconds();
    }

    int64 TimeSeries::CoarsestBucketMilliseconds() const noexcept
    {
        return _tiers.empty() ? 0 : _tiers.back().BucketMilliseconds();
    }

    std::vector<std::pair<int64, std::vector<SeriesBucket>>> TimeSeries::Export() const
    {
        std::vector<std::pair<int64, std::vector<SeriesBucket>>> tiers;
        tiers.reserve(_tiers.size());
        for (SeriesTier const& tier : _tiers)
            tiers.emplace_back(tier.BucketMilliseconds(), tier.Export());
        return tiers;
    }

    void TimeSeries::Restore(std::vector<std::pair<int64, std::vector<SeriesBucket>>> const& tiers)
    {
        for (auto const& [bucketMilliseconds, buckets] : tiers)
        {
            for (SeriesTier& tier : _tiers)
            {
                if (tier.BucketMilliseconds() == bucketMilliseconds)
                {
                    tier.Restore(buckets);
                    break;
                }
            }
        }
    }

    void TimeSeries::Add(int64 atMilliseconds, double value)
    {
        for (SeriesTier& tier : _tiers)
            tier.Add(atMilliseconds, value);
    }

    std::vector<SeriesPoint> TimeSeries::Group(std::vector<SeriesPoint> const& points, std::size_t mostPoints, int64 bucketMilliseconds)
    {
        if (mostPoints == 0 || points.size() <= mostPoints)
            return points;
        std::size_t const perGroup = (points.size() + mostPoints - 1) / mostPoints;
        std::vector<SeriesPoint> grouped;
        grouped.reserve((points.size() + perGroup - 1) / perGroup);
        for (std::size_t start = 0; start < points.size(); start += perGroup)
        {
            std::size_t const stop = std::min(start + perGroup, points.size());
            SeriesPoint made;
            made.AtMilliseconds = points[start].AtMilliseconds;
            double sum = 0.0;
            for (std::size_t at = start; at < stop; ++at)
            {
                SeriesPoint const& point = points[at];
                if (!point.Present)
                    continue;
                if (!made.Present)
                {
                    made.Present = true;
                    made.Lowest = point.Lowest;
                    made.Highest = point.Highest;
                }
                sum += point.Mean * point.Samples;
                made.Lowest = std::min(made.Lowest, point.Lowest);
                made.Highest = std::max(made.Highest, point.Highest);
                made.Samples += point.Samples;
            }
            if (made.Present && made.Samples > 0)
                made.Mean = sum / made.Samples;
            grouped.push_back(made);
        }
        (void)bucketMilliseconds;
        return grouped;
    }

    std::vector<SeriesPoint> TimeSeries::Between(int64 fromMilliseconds, int64 toMilliseconds, std::size_t mostPoints) const
    {
        if (_tiers.empty() || toMilliseconds < fromMilliseconds)
            return {};

        int64 const wanted = toMilliseconds - fromMilliseconds;
        std::vector<SeriesPoint> best;
        bool chosen = false;
        for (SeriesTier const& tier : _tiers)
        {
            bool const reaches = tier.Span() >= wanted;
            bool const bounded = mostPoints == 0 || wanted / tier.BucketMilliseconds() + 1 <= static_cast<int64>(mostPoints);
            if (!reaches || !bounded)
                continue;
            std::vector<SeriesPoint> points = tier.Between(fromMilliseconds, toMilliseconds);
            bool const anything = std::any_of(points.begin(), points.end(),
                [](SeriesPoint const& point) { return point.Present; });
            if (!chosen)
            {
                best = points;
                chosen = true;
            }
            if (anything)
            {
                best = std::move(points);
                break;
            }
        }
        if (!chosen)
            best = _tiers.back().Between(fromMilliseconds, toMilliseconds);
        return Group(best, mostPoints, FinestBucketMilliseconds());
    }
}
