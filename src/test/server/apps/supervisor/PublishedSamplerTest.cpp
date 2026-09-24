/*
 * Project Ambrose by Imjustchico
 * Checks what the published sampler promises: a gauge is written as it stands because it already describes a moment, a counter becomes what it counted per second between two readings rather than its total since the server started, a histogram becomes the mean of the observations made in that stretch rather than of every one since boot, the first reading of an app writes only its gauges because a total needs two readings to mean anything, labels are folded into the name so two pools are two histories rather than one sum, a total that went backwards because the app restarted is skipped rather than drawn as a negative rate, an app with no process is skipped and forgotten, and an answer that is not the JSON this expects disturbs nothing already recorded.
 */

#include "PublishedSampler.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using namespace Ambrose;

    AppSnapshot Running(std::string name, int64 processId = 4242)
    {
        AppSnapshot app;
        app.Name = std::move(name);
        app.ProcessId = processId;
        app.State = AppState::Running;
        return app;
    }

    AppSnapshot Stopped(std::string name)
    {
        AppSnapshot app;
        app.Name = std::move(name);
        app.State = AppState::Offline;
        return app;
    }

    std::string Answer(std::string const& metrics)
    {
        return std::string(R"({"schema":1,"metrics":[)") + metrics + "]}";
    }

    std::string Gauge(char const* name, double value)
    {
        return std::string(R"({"name":")") + name + R"(","kind":"gauge","series":[{"labels":{},"value":)"
            + std::to_string(value) + "}]}";
    }

    std::string Counter(char const* name, double value)
    {
        return std::string(R"({"name":")") + name + R"(","kind":"counter","series":[{"labels":{},"value":)"
            + std::to_string(value) + "}]}";
    }

    std::string Histogram(char const* name, double sum, double count)
    {
        return std::string(R"({"name":")") + name + R"(","kind":"histogram","series":[{"labels":{},"sum":)"
            + std::to_string(sum) + R"(,"count":)" + std::to_string(count) + "}]}";
    }

    double Latest(SeriesStore const& store, std::string const& app, std::string const& series)
    {
        std::vector<SeriesPoint> const points = store.Between(app, series, 0, 600000, 500);
        for (auto at = points.rbegin(); at != points.rend(); ++at)
        {
            if (at->Present)
                return at->Mean;
        }
        return -1.0;
    }
}

TEST(PublishedSamplerTest, AGaugeIsWrittenAsItStands)
{
    SeriesStore store;
    PublishedSampler sampler(store, [](std::string const&) { return Answer(Gauge("ambrose_sessions", 12)); });

    sampler.Sample({ Running("gameserver") }, 0);
    EXPECT_TRUE(store.Has("gameserver", "sessions")) << "the ambrose prefix belongs to the scrape, not to a panel label";
    EXPECT_DOUBLE_EQ(Latest(store, "gameserver", "sessions"), 12.0);
}

TEST(PublishedSamplerTest, TheFirstReadingOfAnAppWritesOnlyItsGauges)
{
    SeriesStore store;
    PublishedSampler sampler(store, [](std::string const&) {
        return Answer(Gauge("ambrose_sessions", 3) + "," + Counter("ambrose_messages_total", 5000));
    });

    sampler.Sample({ Running("gameserver") }, 0);
    EXPECT_TRUE(store.Has("gameserver", "sessions"));
    EXPECT_FALSE(store.Has("gameserver", "messages_total_per_second"))
        << "one total says nothing about a rate, and the total since boot is not what an operator is watching";
}

TEST(PublishedSamplerTest, ACounterBecomesWhatItCountedPerSecond)
{
    SeriesStore store;
    double total = 1000;
    PublishedSampler sampler(store, [&total](std::string const&) { return Answer(Counter("ambrose_messages_total", total)); });

    sampler.Sample({ Running("gameserver") }, 0);
    total = 1500;
    sampler.Sample({ Running("gameserver") }, 5000);

    ASSERT_TRUE(store.Has("gameserver", "messages_total_per_second"));
    EXPECT_DOUBLE_EQ(Latest(store, "gameserver", "messages_total_per_second"), 100.0)
        << "five hundred more over five seconds is a hundred a second";
}

TEST(PublishedSamplerTest, AHistogramBecomesTheMeanOfTheStretchRatherThanOfEverySinceBoot)
{
    SeriesStore store;
    double sum = 100.0;
    double count = 10000.0;
    PublishedSampler sampler(store, [&sum, &count](std::string const&) {
        return Answer(Histogram("ambrose_world_tick_seconds", sum, count));
    });

    sampler.Sample({ Running("gameserver") }, 0);
    sum = 100.4;
    count = 10010.0;
    sampler.Sample({ Running("gameserver") }, 5000);

    ASSERT_TRUE(store.Has("gameserver", "world_tick_seconds_mean"));
    EXPECT_NEAR(Latest(store, "gameserver", "world_tick_seconds_mean"), 0.04, 0.0001)
        << "ten ticks taking four tenths of a second between them is forty milliseconds each, not the ten milliseconds of the lifetime mean";
}

TEST(PublishedSamplerTest, LabelsAreFoldedIntoTheNameSoTwoPoolsAreTwoHistories)
{
    SeriesStore store;
    PublishedSampler sampler(store, [](std::string const&) {
        return Answer(std::string(R"({"name":"ambrose_database_connections","kind":"gauge","series":[)")
            + R"({"labels":{"pool":"login"},"value":2},{"labels":{"pool":"characters"},"value":5}]})");
    });

    sampler.Sample({ Running("loginserver") }, 0);
    EXPECT_TRUE(store.Has("loginserver", "database_connections_login"));
    EXPECT_TRUE(store.Has("loginserver", "database_connections_characters"));
    EXPECT_DOUBLE_EQ(Latest(store, "loginserver", "database_connections_login"), 2.0);
    EXPECT_DOUBLE_EQ(Latest(store, "loginserver", "database_connections_characters"), 5.0)
        << "one history holding the sum of two pools would hide which one moved";
}

TEST(PublishedSamplerTest, ATotalThatWentBackwardsIsSkippedRatherThanDrawnAsANegativeRate)
{
    SeriesStore store;
    double total = 9000;
    PublishedSampler sampler(store, [&total](std::string const&) { return Answer(Counter("ambrose_messages_total", total)); });

    sampler.Sample({ Running("gameserver") }, 0);
    total = 40;
    sampler.Sample({ Running("gameserver") }, 5000);

    EXPECT_FALSE(store.Has("gameserver", "messages_total_per_second"))
        << "the app restarted and began again from nothing, which is not a rate of minus eighteen hundred a second";
}

TEST(PublishedSamplerTest, AnAppWithNoProcessIsSkippedAndForgotten)
{
    SeriesStore store;
    int asked = 0;
    double total = 100;
    PublishedSampler sampler(store, [&asked, &total](std::string const&) {
        ++asked;
        return Answer(Counter("ambrose_messages_total", total));
    });

    sampler.Sample({ Running("gameserver") }, 0);
    EXPECT_EQ(asked, 1);
    sampler.Sample({ Stopped("gameserver") }, 5000);
    EXPECT_EQ(asked, 1) << "a stopped app is not asked, so nothing waits on a socket that will not answer";

    total = 600;
    sampler.Sample({ Running("gameserver") }, 10000);
    EXPECT_FALSE(store.Has("gameserver", "messages_total_per_second"))
        << "the reading from before it stopped belongs to the process that has gone";
}

TEST(PublishedSamplerTest, AnAnswerThatIsNotWhatThisExpectsDisturbsNothing)
{
    SeriesStore store;
    std::string body = Answer(Gauge("ambrose_sessions", 7));
    PublishedSampler sampler(store, [&body](std::string const&) { return body; });

    sampler.Sample({ Running("gameserver") }, 0);
    ASSERT_DOUBLE_EQ(Latest(store, "gameserver", "sessions"), 7.0);

    body = "this is not JSON";
    sampler.Sample({ Running("gameserver") }, 5000);
    EXPECT_DOUBLE_EQ(Latest(store, "gameserver", "sessions"), 7.0) << "what was recorded stays recorded";

    body = R"({"schema":1})";
    sampler.Sample({ Running("gameserver") }, 10000);
    EXPECT_DOUBLE_EQ(Latest(store, "gameserver", "sessions"), 7.0);
}

TEST(PublishedSamplerTest, AnAppThatDoesNotAnswerIsSkipped)
{
    SeriesStore store;
    PublishedSampler sampler(store, [](std::string const&) { return std::optional<std::string>(); });

    std::vector<std::string> const read = sampler.Sample({ Running("gameserver") }, 0);
    EXPECT_TRUE(read.empty());
    EXPECT_EQ(store.Count(), 0u);
}

TEST(PublishedSamplerTest, AHistogramNothingHasBeenObservedInIsWrittenNowhere)
{
    SeriesStore store;
    PublishedSampler sampler(store, [](std::string const&) {
        return Answer(Histogram("ambrose_reload_seconds", 0.0, 0.0));
    });

    sampler.Sample({ Running("gameserver") }, 0);
    sampler.Sample({ Running("gameserver") }, 5000);

    EXPECT_FALSE(store.Has("gameserver", "reload_seconds_mean"))
        << "there is no mean of no observations";
    EXPECT_FALSE(store.Has("gameserver", "reload_seconds_per_second"))
        << "and a histogram is never a rate, however many observations it happens to have, because what it is was named by the app";
    EXPECT_EQ(store.Count(), 0u);
}

TEST(PublishedSamplerTest, AMetricThatChangedItsKindIsNotComparedAcrossTheChange)
{
    SeriesStore store;
    bool histogram = true;
    PublishedSampler sampler(store, [&histogram](std::string const&) {
        return histogram ? Answer(Histogram("ambrose_thing", 10.0, 5.0)) : Answer(Counter("ambrose_thing", 10.0));
    });

    sampler.Sample({ Running("gameserver") }, 0);
    histogram = false;
    sampler.Sample({ Running("gameserver") }, 5000);

    EXPECT_FALSE(store.Has("gameserver", "thing_per_second"))
        << "a total that used to mean something else is not a total to difference against";
    EXPECT_FALSE(store.Has("gameserver", "thing_mean"));
}
