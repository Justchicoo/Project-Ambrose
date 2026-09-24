/*
 * Project Ambrose by Imjustchico
 * Checks what a metric promises: a counter counts every increment exactly, including from many threads at once, a gauge moves both ways, a histogram puts an observation in the bucket it belongs to and keeps the sum and count beside it, the registry hands back the same metric for a name rather than making a second one, a name the Prometheus format would refuse never reaches the scrape, and the text a scrape produces carries the HELP and TYPE lines and the cumulative buckets a reader needs. Checks too that an update is cheap enough to sit in a hot path, which is the reason these are atomics rather than anything guarded.
 */

#include "Metric.h"
#include "MetricRegistry.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace Ambrose;

    class MetricRegistryTest : public testing::Test
    {
    protected:
        void SetUp() override { sMetrics.Clear(); }
        void TearDown() override { sMetrics.Clear(); }
    };
}

TEST_F(MetricRegistryTest, ACounterCountsEveryIncrementExactly)
{
    Counter counter;
    EXPECT_EQ(counter.Value(), 0u);
    for (int made = 0; made < 1000; ++made)
        counter.Add();
    EXPECT_EQ(counter.Value(), 1000u) << "a thousand messages must move the counter by exactly a thousand";
    counter.Add(5);
    EXPECT_EQ(counter.Value(), 1005u);
}

TEST_F(MetricRegistryTest, ACounterLosesNothingWhenManyThreadsCount)
{
    Counter counter;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread)
        threads.emplace_back([&counter] { for (int made = 0; made < 25000; ++made) counter.Add(); });
    for (std::thread& thread : threads)
        thread.join();
    EXPECT_EQ(counter.Value(), 100000u) << "four threads counting must add up to what they counted";
}

TEST_F(MetricRegistryTest, AGaugeMovesBothWays)
{
    Gauge gauge;
    gauge.Set(10);
    EXPECT_EQ(gauge.Value(), 10);
    gauge.Add(5);
    gauge.Subtract(3);
    EXPECT_EQ(gauge.Value(), 12);
    gauge.Set(0);
    EXPECT_EQ(gauge.Value(), 0);
}

TEST_F(MetricRegistryTest, AHistogramPutsAnObservationWhereItBelongs)
{
    Histogram histogram;
    histogram.Observe(0.004);
    histogram.Observe(0.2);
    histogram.Observe(30.0);

    EXPECT_EQ(histogram.Count(), 3u);
    EXPECT_NEAR(histogram.Sum(), 30.204, 0.0001);
    EXPECT_EQ(histogram.Cumulative(0), 1u) << "the 5ms bucket holds the 4ms observation";
    EXPECT_EQ(histogram.Cumulative(5), 2u) << "and by 250ms it holds the 200ms one as well";
    EXPECT_EQ(histogram.Cumulative(Histogram::BucketCount - 1), 2u) << "an observation past the last bound is in none of them";
}

TEST_F(MetricRegistryTest, ANameIsMadeOnceAndHandedBackAfterwards)
{
    Counter& first = sMetrics.CounterFor("ambrose_messages_total", "Messages handled");
    first.Add(7);
    Counter& again = sMetrics.CounterFor("ambrose_messages_total", "Messages handled");
    EXPECT_EQ(&first, &again) << "two subsystems counting one name must count the same thing";
    EXPECT_EQ(again.Value(), 7u) << "and the second caller must not be handed a fresh zero";
    EXPECT_EQ(sMetrics.Count(), 1u);
}

TEST_F(MetricRegistryTest, ANameTheFormatWouldRefuseNeverReachesAScrape)
{
    EXPECT_TRUE(MetricRegistry::IsNameAllowed("ambrose_sessions"));
    EXPECT_TRUE(MetricRegistry::IsNameAllowed("_leading_underscore"));
    EXPECT_FALSE(MetricRegistry::IsNameAllowed(""));
    EXPECT_FALSE(MetricRegistry::IsNameAllowed("1_starts_with_a_digit"));
    EXPECT_FALSE(MetricRegistry::IsNameAllowed("has a space"));
    EXPECT_FALSE(MetricRegistry::IsNameAllowed("has-a-dash"));

    sMetrics.CounterFor("has a space", "Refused").Add();
    EXPECT_EQ(MetricRegistry::Expose(sMetrics.Collect()).find("has a space"), std::string::npos)
        << "one bad name must not make the whole scrape unreadable";
}

TEST_F(MetricRegistryTest, TheScrapeCarriesHelpTypeAndTheCumulativeBuckets)
{
    sMetrics.CounterFor("ambrose_messages_total", "Messages handled").Add(3);
    sMetrics.GaugeFor("ambrose_sessions", "Sessions connected").Set(2);
    sMetrics.HistogramFor("ambrose_tick_seconds", "How long a tick took").Observe(0.02);

    std::string const text = MetricRegistry::Expose(sMetrics.Collect());

    EXPECT_NE(text.find("# HELP ambrose_messages_total Messages handled"), std::string::npos);
    EXPECT_NE(text.find("# TYPE ambrose_messages_total counter"), std::string::npos);
    EXPECT_NE(text.find("ambrose_messages_total 3"), std::string::npos);

    EXPECT_NE(text.find("# TYPE ambrose_sessions gauge"), std::string::npos);
    EXPECT_NE(text.find("ambrose_sessions 2"), std::string::npos);

    EXPECT_NE(text.find("# TYPE ambrose_tick_seconds histogram"), std::string::npos);
    EXPECT_NE(text.find("ambrose_tick_seconds_bucket{le=\"0.025\"} 1"), std::string::npos) << text;
    EXPECT_NE(text.find("ambrose_tick_seconds_bucket{le=\"+Inf\"} 1"), std::string::npos);
    EXPECT_NE(text.find("ambrose_tick_seconds_count 1"), std::string::npos);
    EXPECT_NE(text.find("ambrose_tick_seconds_sum"), std::string::npos);
}

TEST_F(MetricRegistryTest, AHelpLineCannotEndTheMetricEarly)
{
    sMetrics.CounterFor("ambrose_awkward_total", "One line\nand another").Add();
    std::string const text = MetricRegistry::Expose(sMetrics.Collect());
    EXPECT_NE(text.find("One line\\nand another"), std::string::npos) << "a newline in help is escaped rather than written raw";
    EXPECT_EQ(text.find("# HELP ambrose_awkward_total One line\nand another"), std::string::npos);
}

TEST_F(MetricRegistryTest, AnUpdateIsCheapEnoughToSitInAHotPath)
{
    Counter counter;
    constexpr int Rounds = 2000000;

    auto const started = std::chrono::steady_clock::now();
    for (int made = 0; made < Rounds; ++made)
        counter.Add();
    auto const took = std::chrono::steady_clock::now() - started;

    ASSERT_EQ(counter.Value(), static_cast<uint64>(Rounds));
    double const nanoseconds = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(took).count()) / Rounds;
    EXPECT_LT(nanoseconds, 50.0) << "an update costs " << nanoseconds << "ns, which is too much to put in a message handler";
}
