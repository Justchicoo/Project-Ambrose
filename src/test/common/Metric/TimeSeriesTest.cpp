/*
 * Project Ambrose by Imjustchico
 * Checks what a history promises: a sample lands in the bucket its time belongs to, a bucket nothing was written into is absent rather than zero so a stopped app leaves a gap, one sample reaches every resolution as it is written rather than waiting for a sweep, a slot the ring has wrapped past reads as absent rather than as a stale value under a new bucket, a sample older than the ring can hold is refused rather than written over a newer point, the lowest and highest survive being folded down so a spike is still visible in a month-long view, and any range asked for comes back within the number of points the caller will take.
 */

#include "TimeSeries.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    using namespace Ambrose;

    constexpr int64 Second = 1000;
    constexpr int64 Minute = 60 * Second;
    constexpr int64 Hour = 60 * Minute;
    constexpr int64 Day = 24 * Hour;

    std::size_t PresentIn(std::vector<SeriesPoint> const& points)
    {
        std::size_t present = 0;
        for (SeriesPoint const& point : points)
            if (point.Present)
                ++present;
        return present;
    }
}

TEST(TimeSeriesTest, ASampleLandsInTheBucketItsTimeBelongsTo)
{
    SeriesTier tier(5 * Second, 100);
    EXPECT_TRUE(tier.Add(0, 10.0));
    EXPECT_TRUE(tier.Add(4999, 20.0));
    EXPECT_TRUE(tier.Add(5000, 40.0));

    std::vector<SeriesPoint> const points = tier.Between(0, 9999);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(points[0].Present);
    EXPECT_EQ(points[0].AtMilliseconds, 0);
    EXPECT_DOUBLE_EQ(points[0].Mean, 15.0) << "two samples in one bucket average";
    EXPECT_EQ(points[0].Samples, 2u);
    EXPECT_TRUE(points[1].Present);
    EXPECT_DOUBLE_EQ(points[1].Mean, 40.0) << "the sample on the boundary belongs to the next bucket";
}

TEST(TimeSeriesTest, ABucketNothingWasWrittenIntoIsAbsentRatherThanZero)
{
    SeriesTier tier(5 * Second, 100);
    tier.Add(0, 10.0);
    tier.Add(20 * Second, 10.0);

    std::vector<SeriesPoint> const points = tier.Between(0, 20 * Second);
    ASSERT_EQ(points.size(), 5u);
    EXPECT_TRUE(points[0].Present);
    EXPECT_FALSE(points[1].Present) << "an app that was stopped drew no samples, and a zero would read as an idle app";
    EXPECT_FALSE(points[2].Present);
    EXPECT_FALSE(points[3].Present);
    EXPECT_TRUE(points[4].Present);
    EXPECT_DOUBLE_EQ(points[1].Mean, 0.0) << "the value of an absent point means nothing and must not be drawn";
}

TEST(TimeSeriesTest, OneSampleReachesEveryResolutionAsItIsWritten)
{
    TimeSeries series({ { 5 * Second, 100 }, { Minute, 100 } });
    for (int at = 0; at < 24; ++at)
        series.Add(at * 5 * Second, 10.0 + at);

    ASSERT_EQ(series.Tiers().size(), 2u);
    EXPECT_EQ(series.Tiers()[0].Held(), 24u) << "twenty-four five-second buckets";
    EXPECT_EQ(series.Tiers()[1].Held(), 2u) << "and the same samples folded into two minutes, without a sweep having run";

    std::vector<SeriesPoint> const coarse = series.Tiers()[1].Between(0, Minute);
    ASSERT_EQ(coarse.size(), 2u);
    ASSERT_TRUE(coarse[0].Present);
    EXPECT_EQ(coarse[0].Samples, 12u);
    EXPECT_DOUBLE_EQ(coarse[0].Lowest, 10.0);
    EXPECT_DOUBLE_EQ(coarse[0].Highest, 21.0);
}

TEST(TimeSeriesTest, ASpikeSurvivesBeingFoldedDown)
{
    TimeSeries series({ { 5 * Second, 100 }, { Minute, 100 } });
    for (int at = 0; at < 12; ++at)
        series.Add(at * 5 * Second, 1.0);
    series.Add(30 * Second, 900.0);

    std::vector<SeriesPoint> const coarse = series.Tiers()[1].Between(0, 0);
    ASSERT_EQ(coarse.size(), 1u);
    ASSERT_TRUE(coarse[0].Present);
    EXPECT_DOUBLE_EQ(coarse[0].Highest, 900.0) << "a spike between two reads must still be visible after folding";
    EXPECT_DOUBLE_EQ(coarse[0].Lowest, 1.0);
    EXPECT_LT(coarse[0].Mean, 900.0) << "and the mean must not be the spike";
}

TEST(TimeSeriesTest, ASlotTheRingHasWrappedPastReadsAsAbsent)
{
    SeriesTier tier(Second, 4);
    tier.Add(0, 1.0);
    tier.Add(Second, 2.0);
    tier.Add(4 * Second, 5.0);

    std::vector<SeriesPoint> const points = tier.Between(0, 4 * Second);
    ASSERT_EQ(points.size(), 5u);
    EXPECT_FALSE(points[0].Present) << "the ring wrapped onto this slot, so the old bucket is gone rather than misread";
    EXPECT_TRUE(points[1].Present);
    EXPECT_DOUBLE_EQ(points[1].Mean, 2.0);
    EXPECT_TRUE(points[4].Present);
    EXPECT_DOUBLE_EQ(points[4].Mean, 5.0);
}

TEST(TimeSeriesTest, ASampleOlderThanTheRingIsRefusedRatherThanWrittenOverANewerPoint)
{
    SeriesTier tier(Second, 4);
    tier.Add(10 * Second, 5.0);
    EXPECT_FALSE(tier.Add(0, 99.0)) << "that bucket left the ring, and its slot now belongs to a newer one";
    EXPECT_TRUE(tier.Add(9 * Second, 4.0)) << "a sample still inside the ring is taken";

    std::vector<SeriesPoint> const points = tier.Between(10 * Second, 10 * Second);
    ASSERT_EQ(points.size(), 1u);
    ASSERT_TRUE(points[0].Present);
    EXPECT_DOUBLE_EQ(points[0].Mean, 5.0) << "the newer point is untouched";
}

TEST(TimeSeriesTest, ARangeComesBackWithinThePointsTheCallerWillTake)
{
    TimeSeries series;
    ASSERT_GE(series.Tiers().size(), 2u);
    EXPECT_EQ(series.FinestBucketMilliseconds(), 5 * Second);

    int64 const now = 40 * Day;
    for (int64 at = now - 3 * Hour; at <= now; at += 5 * Second)
        series.Add(at, 50.0);

    std::vector<SeriesPoint> const short_ = series.Between(now - 5 * Minute, now, 300);
    EXPECT_LE(short_.size(), 300u);
    EXPECT_GT(PresentIn(short_), 0u) << "five minutes must still carry its detail";

    std::vector<SeriesPoint> const long_ = series.Between(now - 30 * Day, now, 500);
    EXPECT_LE(long_.size(), 500u) << "a month must read a bounded number of points";
    EXPECT_GT(PresentIn(long_), 0u) << "and must still show the three hours that were written";
}

TEST(TimeSeriesTest, AGroupedRangeKeepsTheExtremesOfWhatItGrouped)
{
    TimeSeries series({ { Second, 1000 } });
    for (int at = 0; at < 100; ++at)
        series.Add(at * Second, at == 47 ? 500.0 : 1.0);

    std::vector<SeriesPoint> const points = series.Between(0, 99 * Second, 10);
    EXPECT_LE(points.size(), 10u);
    double highest = 0.0;
    for (SeriesPoint const& point : points)
        if (point.Present)
            highest = std::max(highest, point.Highest);
    EXPECT_DOUBLE_EQ(highest, 500.0) << "grouping for the screen must not hide the spike";
}
