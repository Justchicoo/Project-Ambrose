/*
 * Project Ambrose by Imjustchico
 * Runs the stream layer's shared cases against the panel's event socket feed, the same ones the live log and the admin events pass, with each record written in the socket's envelope and its sequence stamped by the hub: a new session gets the hello and the backlog in order, a resume after N gets exactly the records after N, an evicted range gets a dropped marker naming it, a resume at the latest gets only the hello, a full queue drops the oldest and names them, sequences never go backwards across pump batches, and the backlog read over HTTP says what it could no longer show. Then the envelope of an app record and of a panel-wide one, the dropped frame's data, the status stream that never drops and closes its session once its queue fills, a message the catalog does not mark as sent on a stream refused with nothing published, the stats stream that keeps only each app's newest sample, a stream request's reading and the filter that follows one app within the apps a caller may see.
 */

#include "PanelEventCatalog.h"
#include "PanelEventFrame.h"
#include "PanelEventStreams.h"
#include "StreamLayerCases.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::string StatusOf(std::string const& app, uint64 sequence)
    {
        return fmt::format(R"({{"app":"{}","state":"running","since":{},"pid":{},"exit_code":null,"crashes":0,"next_restart":null}})", app, 1789650000000 + sequence, 4000 + sequence);
    }

    struct PanelFeed
    {
        using Hub = PanelEventHub;
        using Service = PanelEventService;
        using Request = PanelEventRequest;
        static constexpr std::string_view RecordType = "status";
        static constexpr std::string_view SequenceField = "seq";
        static constexpr std::array<std::string_view, 1> StampFields{ "time" };

        static void PublishRange(PanelEventHub& hub, uint64 from, uint64 to)
        {
            for (uint64 sequence = from; sequence <= to; ++sequence)
            {
                ASSERT_EQ(hub.Announce("status", "gameserver-1", StatusOf("gameserver-1", sequence), "gameserver-1"), sequence)
                    << "the hub stamps each record with the next sequence number";
            }
        }
    };

    using Cases = StreamLayerCases<PanelFeed>;

    class PanelEventStreamTest : public testing::Test
    {
    protected:
        PanelEventHub _hub;
        PanelEventService _service{ _hub };
    };

    void PumpAll(PanelEventService& service)
    {
        while (service.Pump() > 0)
        {
        }
    }
}

TEST_F(PanelEventStreamTest, ANewSessionGetsTheHelloThenTheWholeBacklogInOrder)
{
    Cases::NewSessionGetsHelloThenBacklogInOrder(_hub, _service);
}

TEST_F(PanelEventStreamTest, AResumeAfterNGetsExactlyTheRecordsAfterN)
{
    Cases::ResumeAfterN(_hub, _service);
}

TEST_F(PanelEventStreamTest, AResumeAfterAnEvictedSequenceGetsADroppedMarkerNamingTheMissedRange)
{
    Cases::ResumeAfterEvicted(_hub, _service);
}

TEST_F(PanelEventStreamTest, AResumeAtTheLatestSequenceGetsNothingButTheHello)
{
    Cases::ResumeAtLatest(_hub, _service);
}

TEST_F(PanelEventStreamTest, AFullQueueDropsTheOldestAndReportsHowManyWithTheirRange)
{
    Cases::FullQueueDrops(_hub, _service);
}

TEST_F(PanelEventStreamTest, SequenceNumbersNeverGoBackwardsAcrossPumpBatches)
{
    Cases::SequenceNeverBackwards(_hub, _service);
}

TEST_F(PanelEventStreamTest, TheBacklogIsReadableOverHttpAfterASequenceNumber)
{
    Cases::BacklogOverHttp(_hub);
}

TEST_F(PanelEventStreamTest, ARecordTravelsInTheEnvelopeWithItsScopeSequenceTimeAndData)
{
    _hub.Announce("status", "gameserver-1", StatusOf("gameserver-1", 1), "gameserver-1");
    _hub.Announce("status", "", "{}", "");
    std::vector<std::shared_ptr<PanelEvent const>> const backlog = _hub.GetBacklog();
    ASSERT_EQ(backlog.size(), 2u);

    std::string const text = PanelEventTraits::Encode(*backlog[0]);
    EXPECT_TRUE(text.starts_with(R"({"v":1,"type":"status","id":null,"scope":{"app":"gameserver-1"},"seq":1,"time":)")) << text;
    nlohmann::json const record = nlohmann::json::parse(text);
    for (std::string_view const field : PanelEventCatalog::EnvelopeFields())
    {
        EXPECT_TRUE(record.contains(std::string(field))) << field;
    }
    EXPECT_TRUE(record["time"].is_number_integer());
    ASSERT_TRUE(record["data"].is_object()) << "data is an object, never text holding one";
    EXPECT_EQ(record["data"]["app"], "gameserver-1");
    std::string error;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "status", record["data"], error)) << error;

    nlohmann::json const panelWide = nlohmann::json::parse(PanelEventTraits::Encode(*backlog[1]));
    EXPECT_TRUE(panelWide["scope"].is_null());
    EXPECT_EQ(panelWide["seq"], 2u);
}

TEST_F(PanelEventStreamTest, ADroppedFrameNamesItsStreamAndTheMissedRange)
{
    std::string const data = PanelEventFrame::DroppedData("status", 21, 40, 20);
    nlohmann::json const parsed = nlohmann::json::parse(data);
    EXPECT_EQ(parsed["stream"], "status");
    EXPECT_EQ(parsed["first"], 21u);
    EXPECT_EQ(parsed["last"], 40u);
    EXPECT_EQ(parsed["count"], 20u);
    std::string error;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "dropped", parsed, error)) << error;

    nlohmann::json const frame = nlohmann::json::parse(PanelEventFrame::Write("dropped", std::nullopt, "gameserver-1", std::nullopt, 1789650000123, data));
    EXPECT_TRUE(frame["seq"].is_null()) << "a dropped frame is not a record of its stream";
    EXPECT_TRUE(frame["id"].is_null());
    EXPECT_EQ(frame["scope"]["app"], "gameserver-1");
    EXPECT_EQ(frame["time"], 1789650000123);
}

TEST_F(PanelEventStreamTest, TheStatusStreamNeverDropsAndClosesItsSessionOnceItsQueueFills)
{
    ASSERT_NE(PanelEventCatalog::FindStream("status"), nullptr);
    EXPECT_EQ(PanelEventCatalog::FindStream("status")->Overflow, StreamOverflow::KeepAll);

    PanelEventStreams streams;
    ASSERT_TRUE(streams.Serves("status"));
    auto const sink = std::make_shared<RecordingStreamSink>("status", "seq");
    PanelEventRequest request;
    request.Stream = "status";
    request.QueueCapacity = 5;
    std::shared_ptr<PanelEventSession> const session = streams.Open(sink, request);
    ASSERT_NE(session, nullptr);

    for (uint64 sequence = 1; sequence <= 12; ++sequence)
    {
        EXPECT_EQ(streams.Publish("status", "status", "gameserver-1", StatusOf("gameserver-1", sequence), "gameserver-1"), sequence);
    }
    ASSERT_NE(streams.Service("status"), nullptr);
    PumpAll(*streams.Service("status"));

    EXPECT_EQ(sink->Sequences(), SequenceRange(1, 5)) << "every record the queue held is delivered before it closes";
    EXPECT_TRUE(sink->OfType("dropped").empty()) << "a stream that never drops sends no dropped marker";
    EXPECT_TRUE(sink->IsClosed());
    EXPECT_EQ(sink->CloseReason(), "overflowed");
    EXPECT_TRUE(session->IsClosed());
    EXPECT_EQ(session->GetDroppedCount(), 0u);
}

TEST_F(PanelEventStreamTest, AMessageTheCatalogDoesNotMarkAsSentOnAStreamIsRefusedAndNothingIsPublished)
{
    PanelEventStreams streams;
    ASSERT_NE(streams.Hub("status"), nullptr);
    EXPECT_THROW(streams.Publish("status", "power.result", "gameserver-1", "{}"), std::invalid_argument) << "a type its milestone has not built reaches no page";
    EXPECT_THROW(streams.Publish("status", "pong", "gameserver-1", "{}"), std::invalid_argument) << "a frame outside every stream is not a record";
    EXPECT_THROW(streams.Publish("logs", "status", "gameserver-1", StatusOf("gameserver-1", 1)), std::invalid_argument) << "a record travels only on its own stream";
    EXPECT_EQ(streams.Hub("status")->GetLatestSequence(), 0u);
    EXPECT_TRUE(streams.Hub("status")->GetBacklog().empty());
    EXPECT_EQ(streams.Publish("status", "status", "gameserver-1", StatusOf("gameserver-1", 1), "gameserver-1"), 1u);
}

TEST_F(PanelEventStreamTest, TheStatsStreamKeepsOnlyEachAppsNewestSample)
{
    ASSERT_NE(PanelEventCatalog::FindStream("stats"), nullptr);
    StreamOverflow const policy = PanelEventCatalog::FindStream("stats")->Overflow;
    EXPECT_EQ(policy, StreamOverflow::NewestPerKey);

    auto const sink = std::make_shared<RecordingStreamSink>("stats", "seq");
    PanelEventRequest request;
    request.Stream = "stats";
    request.Overflow = policy;
    _service.Open(sink, request);
    for (char const* const app : { "gameserver-1", "loginserver", "gameserver-1", "gameserver-1", "loginserver" })
    {
        _hub.Announce("stats", app, R"({"cpu":1})", app);
    }
    PumpAll(_service);

    EXPECT_EQ(sink->Sequences(), (std::vector<uint64>{ 4, 5 }));
    EXPECT_TRUE(sink->OfType("dropped").empty()) << "a sample replaced by a newer one is not a gap";
}

TEST_F(PanelEventStreamTest, AStreamRequestNamesItsStreamAppAndResumePointAndNothingElse)
{
    std::string error;
    std::optional<PanelEventRequest> const request = PanelEventTraits::Parse(R"({"stream":"status","app":"gameserver-1","after":42})", error);
    ASSERT_TRUE(request.has_value()) << error;
    EXPECT_EQ(request->Stream, "status");
    EXPECT_EQ(request->App, std::optional<std::string>("gameserver-1"));
    EXPECT_EQ(request->After, std::optional<uint64>(42));

    EXPECT_FALSE(PanelEventTraits::Parse("not json", error).has_value());
    EXPECT_FALSE(PanelEventTraits::Parse(R"({"stream":"weather"})", error).has_value());
    EXPECT_FALSE(PanelEventTraits::Parse(R"({"stream":"status","after":-1})", error).has_value());
    EXPECT_FALSE(PanelEventTraits::Parse(R"({"stream":"status","level":"warn"})", error).has_value());
    EXPECT_NE(error.find("level"), std::string::npos) << error;
    EXPECT_FALSE(PanelEventTraits::Parse("{}", error).has_value()) << "a request names its stream";
}

TEST_F(PanelEventStreamTest, AFilterFollowsOneAppWithinTheAppsItsCallerMaySee)
{
    PanelEvent game;
    game.App = "gameserver-1";
    PanelEvent login;
    login.App = "loginserver";
    PanelEvent panelWide;

    PanelEventFilter const everything;
    EXPECT_TRUE(everything.Matches(game));
    EXPECT_TRUE(everything.Matches(panelWide));

    PanelEventFilter const one{ std::string("gameserver-1"), std::nullopt };
    EXPECT_TRUE(one.Matches(game));
    EXPECT_FALSE(one.Matches(login));
    EXPECT_FALSE(one.Matches(panelWide));

    PanelEventFilter const visible{ std::nullopt, std::set<std::string, std::less<>>{ "loginserver" } };
    EXPECT_FALSE(visible.Matches(game));
    EXPECT_TRUE(visible.Matches(login));
    EXPECT_TRUE(visible.Matches(panelWide));
}
