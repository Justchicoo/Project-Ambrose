/*
 * Project Ambrose by Imjustchico
 * Tests uptime history and incident timeline 17.94: a stretch of probe failures appears as downtime with the right start and end and the percentages follow, a planned window is shown as planned and never as an incident, the extended payload carries no player name, address or account figure, and the public page and the operator answer carry the same figures.
 */

#include "PanelMaintenance.h"
#include "PanelPublicStatus.h"
#include "PanelUptimeHistory.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace
{
    constexpr int64 kNow = 1000000;

    ProbeSample Sample(int64 epochMs, std::string target, bool ok)
    {
        ProbeSample sample;
        sample.EpochMs = epochMs;
        sample.Target = std::move(target);
        sample.Ok = ok;
        return sample;
    }

    std::vector<ProbeSample> FailureStretch()
    {
        return {
            Sample(10000, "Azeroth", true),
            Sample(20000, "Azeroth", true),
            Sample(30000, "Azeroth", true),
            Sample(40000, "Azeroth", true),
            Sample(50000, "Azeroth", true),
            Sample(60000, "Azeroth", false),
            Sample(70000, "Azeroth", false),
            Sample(80000, "Azeroth", false),
            Sample(90000, "Azeroth", true),
            Sample(15000, "login", true),
            Sample(85000, "login", true),
        };
    }

    nlohmann::json BaseAnswer()
    {
        PublicSourceState source;
        source.LoginServerUp = true;
        source.Realms = { { "Azeroth", true } };
        MaintenanceState maintenance;
        PublicIncident incident;
        return PanelPublicStatus::Answer(source, maintenance, incident, kNow);
    }

    nlohmann::json ExtendedPayload()
    {
        std::vector<ProbeSample> const samples = FailureStretch();
        UptimeSummary const summary = PanelUptimeHistory::ComputeSummary(samples, { "Azeroth" }, kNow);
        std::vector<TimelineIncident> const timeline = PanelUptimeHistory::BuildTimeline(samples, {}, "", kNow);
        std::vector<RealmSparkline> const sparklines = PanelUptimeHistory::BuildSparklines(samples, { "Azeroth" }, kNow);
        nlohmann::json payload = BaseAnswer();
        PanelUptimeHistory::AppendPublicFields(payload, summary, timeline, sparklines);
        return payload;
    }
}

TEST(PanelUptimeHistory, ProbeFailureStretchBecomesDowntimeWithRightStartAndEnd)
{
    std::vector<ProbeSample> const samples = FailureStretch();
    std::vector<TimelineIncident> const timeline = PanelUptimeHistory::BuildTimeline(samples, {}, "", kNow);
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_EQ(timeline[0].StartEpochMs, 60000);
    EXPECT_EQ(timeline[0].EndEpochMs, 80000);
    EXPECT_FALSE(timeline[0].Planned);
    EXPECT_EQ(timeline[0].Cause, "The probe could not reach Azeroth");

    UptimeSummary const summary = PanelUptimeHistory::ComputeSummary(samples, { "Azeroth" }, kNow);
    ASSERT_TRUE(summary.Realms.contains("Azeroth"));
    EXPECT_TRUE(summary.Realms.at("Azeroth").Day.Known);
    EXPECT_DOUBLE_EQ(summary.Realms.at("Azeroth").Day.Percent, 100.0 * 6 / 9);
    EXPECT_DOUBLE_EQ(summary.Realms.at("Azeroth").Month.Percent, 100.0 * 6 / 9);
    EXPECT_DOUBLE_EQ(summary.Realms.at("Azeroth").Year.Percent, 100.0 * 6 / 9);
    EXPECT_TRUE(summary.Login.Day.Known);
    EXPECT_DOUBLE_EQ(summary.Login.Day.Percent, 100.0);
}

TEST(PanelUptimeHistory, PlannedWindowIsShownAsPlannedNotAsIncident)
{
    std::vector<ProbeSample> const samples = FailureStretch();
    PlannedWindow window;
    window.StartEpochMs = 55000;
    window.EndEpochMs = 85000;
    window.Reason = "Weekly restart";
    std::vector<TimelineIncident> const timeline =
        PanelUptimeHistory::BuildTimeline(samples, { window }, "", kNow);
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_TRUE(timeline[0].Planned);
    EXPECT_EQ(timeline[0].StartEpochMs, 55000);
    EXPECT_EQ(timeline[0].EndEpochMs, 85000);
    EXPECT_EQ(timeline[0].Cause, "Weekly restart");
}

TEST(PanelUptimeHistory, OngoingFailureEndsAtNow)
{
    std::vector<ProbeSample> const samples = {
        Sample(10000, "Azeroth", true),
        Sample(20000, "Azeroth", false),
        Sample(30000, "Azeroth", false),
    };
    std::vector<TimelineIncident> const timeline = PanelUptimeHistory::BuildTimeline(samples, {}, "", kNow);
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_EQ(timeline[0].StartEpochMs, 20000);
    EXPECT_EQ(timeline[0].EndEpochMs, kNow);
}

TEST(PanelUptimeHistory, OperatorNoteLandsOnTheLatestEntry)
{
    std::vector<ProbeSample> const samples = FailureStretch();
    std::vector<TimelineIncident> const timeline =
        PanelUptimeHistory::BuildTimeline(samples, {}, "Restarting the world server", kNow);
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_EQ(timeline[0].Note, "Restarting the world server");
}

TEST(PanelUptimeHistory, UnknownWhenNoSamples)
{
    UptimeSummary const summary = PanelUptimeHistory::ComputeSummary({}, { "Azeroth" }, kNow);
    EXPECT_FALSE(summary.Realms.at("Azeroth").Day.Known);
    EXPECT_FALSE(summary.Login.Month.Known);
    nlohmann::json const uptime = PanelUptimeHistory::UptimeJson(summary);
    EXPECT_TRUE(uptime["realms"]["Azeroth"]["day"].is_null());
    EXPECT_TRUE(uptime["login"]["month"].is_null());
    EXPECT_EQ(uptime["computed_epoch_ms"], kNow);
}

TEST(PanelUptimeHistory, SparklineBucketsReflectSamples)
{
    int64 const now = 24 * PanelUptimeHistory::MsPerHour;
    std::vector<ProbeSample> const samples = {
        Sample(now - 1000, "Azeroth", true),
        Sample(now - PanelUptimeHistory::MsPerHour - 500, "Azeroth", false),
        Sample(now - PanelUptimeHistory::MsPerHour - 1500, "Azeroth", true),
    };
    std::vector<RealmSparkline> const sparklines = PanelUptimeHistory::BuildSparklines(samples, { "Azeroth" }, now);
    ASSERT_EQ(sparklines.size(), 1);
    ASSERT_EQ(sparklines[0].Buckets.size(), PanelUptimeHistory::SparklineBuckets);
    ASSERT_TRUE(sparklines[0].Buckets[23].has_value());
    EXPECT_DOUBLE_EQ(*sparklines[0].Buckets[23], 1.0);
    ASSERT_TRUE(sparklines[0].Buckets[22].has_value());
    EXPECT_DOUBLE_EQ(*sparklines[0].Buckets[22], 0.5);
    EXPECT_FALSE(sparklines[0].Buckets[0].has_value());
}

TEST(PanelUptimeHistory, ExtendedPayloadKeepsTheAggregateOnlyShape)
{
    nlohmann::json const payload = ExtendedPayload();
    std::string offending;
    EXPECT_TRUE(PanelUptimeHistory::CheckShaped(payload, offending)) << offending;
    EXPECT_TRUE(payload.contains("uptime"));
    EXPECT_TRUE(payload.contains("incidents"));
    EXPECT_TRUE(payload.contains("sparklines"));
    EXPECT_EQ(payload["incidents"].size(), 1);
    EXPECT_EQ(payload["sparklines"]["Azeroth"].size(), PanelUptimeHistory::SparklineBuckets);
}

TEST(PanelUptimeHistory, ShapingRefusesPlayerDataInNewFields)
{
    std::string offending;
    nlohmann::json payload = ExtendedPayload();
    payload["player_count"] = 42;
    EXPECT_FALSE(PanelUptimeHistory::CheckShaped(payload, offending));
    EXPECT_EQ(offending, "player_count");

    payload = ExtendedPayload();
    payload["uptime"]["realms"]["Azeroth"]["address"] = "10.0.0.1";
    EXPECT_FALSE(PanelUptimeHistory::CheckShaped(payload, offending));
    EXPECT_EQ(offending, "uptime.realms.Azeroth.address");

    payload = ExtendedPayload();
    payload["incidents"][0]["account_id"] = 7;
    EXPECT_FALSE(PanelUptimeHistory::CheckShaped(payload, offending));
    EXPECT_EQ(offending, "account_id");

    payload = ExtendedPayload();
    payload["sparklines"]["Azeroth"].push_back("up");
    EXPECT_FALSE(PanelUptimeHistory::CheckShaped(payload, offending));
}

TEST(PanelUptimeHistory, PublicAndPanelCarryTheSameFigures)
{
    std::vector<ProbeSample> const samples = FailureStretch();
    UptimeSummary const summary = PanelUptimeHistory::ComputeSummary(samples, { "Azeroth" }, kNow);
    std::vector<TimelineIncident> const timeline = PanelUptimeHistory::BuildTimeline(samples, {}, "", kNow);
    std::vector<RealmSparkline> const sparklines = PanelUptimeHistory::BuildSparklines(samples, { "Azeroth" }, kNow);

    nlohmann::json publicPayload = BaseAnswer();
    PanelUptimeHistory::AppendPublicFields(publicPayload, summary, timeline, sparklines);
    nlohmann::json const operatorPayload =
        PanelUptimeHistory::OperatorAnswer(summary, timeline, sparklines, kNow);

    std::string offending;
    EXPECT_TRUE(PanelUptimeHistory::CheckShaped(publicPayload, offending)) << offending;
    EXPECT_TRUE(PanelUptimeHistory::CheckOperatorShaped(operatorPayload, offending)) << offending;
    EXPECT_EQ(publicPayload["uptime"], operatorPayload["uptime"]);
    EXPECT_EQ(publicPayload["incidents"], operatorPayload["incidents"]);
    EXPECT_EQ(publicPayload["sparklines"], operatorPayload["sparklines"]);
    EXPECT_DOUBLE_EQ(publicPayload["uptime"]["realms"]["Azeroth"]["day"].get<double>(), 100.0 * 6 / 9);
}
