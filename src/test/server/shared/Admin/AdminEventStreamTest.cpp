/*
 * Project Ambrose by Imjustchico
 * Runs the stream layer's shared cases against the admin events feed, the same ones the live log passes, with every event's sequence stamped by the hub itself: a new session gets the hello and the backlog in order, a resume after N gets exactly the events after N, an evicted range gets a dropped marker naming it, a resume at the latest gets only the hello, a full queue drops the oldest and names them, sequences never go backwards across pump batches, and the backlog read over HTTP says what it could no longer show; then that a kind filter matches at dots, an event carries its kind, subject and data, a bad subscribe message names its fault, and a secret setting's change is announced masked while a reload's result carries every error.
 */

#include "AdminEvents.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "ReloadMgr.h"
#include "Settings.h"
#include "StreamLayerCases.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct EventSource
    {
        using Hub = AdminEventHub;
        using Service = AdminEventService;
        using Request = AdminEventRequest;
        static constexpr std::string_view RecordType = "event";

        static void PublishRange(AdminEventHub& hub, uint64 from, uint64 to)
        {
            for (uint64 sequence = from; sequence <= to; ++sequence)
                ASSERT_EQ(hub.Announce("setting.changed", "World.UpdateInterval", fmt::format(R"({{"new":"{}"}})", sequence)), sequence)
                    << "the hub stamps each event with the next sequence number";
        }
    };

    using Cases = StreamLayerCases<EventSource>;

    class AdminEventStreamTest : public testing::Test
    {
    protected:
        AdminEventHub _hub;
        AdminEventService _service{ _hub };
    };
}

TEST_F(AdminEventStreamTest, ANewSessionGetsTheHelloThenTheWholeBacklogInOrder)
{
    Cases::NewSessionGetsHelloThenBacklogInOrder(_hub, _service);
}

TEST_F(AdminEventStreamTest, AResumeAfterNGetsExactlyTheEventsAfterN)
{
    Cases::ResumeAfterN(_hub, _service);
}

TEST_F(AdminEventStreamTest, AResumeAfterAnEvictedSequenceGetsADroppedMarkerNamingTheMissedRange)
{
    Cases::ResumeAfterEvicted(_hub, _service);
}

TEST_F(AdminEventStreamTest, AResumeAtTheLatestSequenceGetsNothingButTheHello)
{
    Cases::ResumeAtLatest(_hub, _service);
}

TEST_F(AdminEventStreamTest, AFullQueueDropsTheOldestAndReportsHowManyWithTheirRange)
{
    Cases::FullQueueDrops(_hub, _service);
}

TEST_F(AdminEventStreamTest, SequenceNumbersNeverGoBackwardsAcrossPumpBatches)
{
    Cases::SequenceNeverBackwards(_hub, _service);
}

TEST_F(AdminEventStreamTest, TheBacklogIsReadableOverHttpAfterASequenceNumber)
{
    Cases::BacklogOverHttp(_hub);
}

TEST_F(AdminEventStreamTest, AKindFilterMatchesWholeKindsAndTheirDottedChildren)
{
    _hub.Announce("setting.changed", "World.UpdateInterval", "{}");
    _hub.Announce("reload.result", "messages", "{}");
    _hub.Announce("settings.other", "x", "{}");
    _hub.Announce("setting", "y", "{}");

    auto const sink = Cases::Sink();
    AdminEventRequest request;
    request.Kinds = { "setting" };
    Cases::Open(_service, sink, request);
    EXPECT_EQ(sink->Sequences(), (std::vector<uint64>{ 1, 4 })) << "setting matches setting.changed and setting, but not settings.other";

    auto const reloads = Cases::Sink();
    AdminEventRequest only;
    only.Kinds = { "reload.result" };
    Cases::Open(_service, reloads, only);
    EXPECT_EQ(reloads->Sequences(), (std::vector<uint64>{ 2 }));
}

TEST_F(AdminEventStreamTest, AnEventCarriesItsKindSubjectAndData)
{
    _hub.Announce("reload.result", "messages", R"({"ok":false,"errors":["one","two"]})");
    auto const sink = Cases::Sink();
    Cases::Open(_service, sink);
    std::vector<nlohmann::json> const events = sink->OfType("event");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0]["kind"], "reload.result");
    EXPECT_EQ(events[0]["subject"], "messages");
    EXPECT_EQ(events[0]["data"]["ok"], false);
    EXPECT_EQ(events[0]["data"]["errors"].size(), 2u);
    EXPECT_TRUE(events[0]["epoch_ms"].is_number_integer());
    EXPECT_EQ(_hub.GetLatestSequence(), 1u);
}

TEST_F(AdminEventStreamTest, ABadSubscribeRequestNamesItsFault)
{
    std::string error;
    EXPECT_FALSE(AdminEventTraits::Parse("not json", error).has_value());
    EXPECT_NE(error.find("JSON object"), std::string::npos) << error;
    EXPECT_FALSE(AdminEventTraits::Parse(R"({"kinds":"setting"})", error).has_value());
    EXPECT_NE(error.find("kinds"), std::string::npos) << error;
    EXPECT_FALSE(AdminEventTraits::Parse(R"({"kinds":[""]})", error).has_value());
    EXPECT_FALSE(AdminEventTraits::Parse(R"({"after":-1})", error).has_value());
    EXPECT_NE(error.find("after"), std::string::npos) << error;
    EXPECT_FALSE(AdminEventTraits::Parse(R"({"level":"warn"})", error).has_value());
    EXPECT_NE(error.find("level"), std::string::npos) << error;

    std::optional<AdminEventRequest> const request = AdminEventTraits::Parse(R"({"kinds":["setting","reload.result"],"after":42})", error);
    ASSERT_TRUE(request.has_value()) << error;
    EXPECT_EQ(request->Kinds, (std::vector<std::string>{ "setting", "reload.result" }));
    EXPECT_EQ(request->After, 42u);
    EXPECT_TRUE(AdminEventTraits::Parse("{}", error).has_value());
}

TEST(AdminEventDataTest, ASecretSettingsChangeIsMaskedAndAReloadCarriesEveryError)
{
    LogTestDirectory directory;
    std::string const key = "1:" + std::string(64, 'a');
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(directory.Write("login.conf", "Account.VerifierKeys = " + key + "\n")).Succeeded());
    Settings settings;
    std::vector<std::string> errors;
    ASSERT_TRUE(settings.DeclareFor(SettingApps::Login, errors));
    std::vector<std::string> warnings;
    ASSERT_TRUE(settings.Start(config, std::make_shared<MemorySettingStore>(), warnings));

    SettingChange const change{ "Account.VerifierKeys", key, key + ",2:" + std::string(64, 'b'), SettingAuthor{ "Merle", 0, "panel" }, "a second key", 1700000000 };
    std::string const text = AdminEventData::SettingChanged(settings, change);
    EXPECT_EQ(text.find("aaaa"), std::string::npos) << text;
    EXPECT_EQ(text.find("bbbb"), std::string::npos) << text;
    nlohmann::json const data = nlohmann::json::parse(text);
    EXPECT_EQ(data["old"], "1:***");
    EXPECT_EQ(data["new"], "1:***,2:***");
    EXPECT_EQ(data["visibility"], "secret");
    EXPECT_EQ(data["who"], "Merle");
    EXPECT_EQ(data["source"], "panel");
    EXPECT_EQ(data["reason"], "a second key");
    EXPECT_EQ(data["layer"], "config");

    ReloadOutcome outcome;
    outcome.Target = "messages";
    outcome.Generation = 3;
    outcome.Ok = false;
    outcome.Errors = { "first fault", "second fault" };
    outcome.FinishedEpochMs = 1700000000123;
    nlohmann::json const reload = nlohmann::json::parse(AdminEventData::ReloadResult(outcome));
    EXPECT_EQ(reload["generation"], 3u);
    EXPECT_EQ(reload["ok"], false);
    EXPECT_EQ(reload["errors"], (std::vector<std::string>{ "first fault", "second fault" }));
    EXPECT_EQ(reload["finished_ms"], 1700000000123);
}
