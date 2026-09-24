/*
 * Project Ambrose by Imjustchico
 * Tests the stream layer without opening a socket: a warn filter passes only warnings and above, the same backlog is readable over HTTP after a sequence number and says what it could no longer show, and a record read that way is the record the socket would have sent, a record carries the place in the code it was written at and the template it was written from while one with neither carries no location rather than a wrong one, a new session gets the hello and the whole backlog in sequence order, a resume after N gets exactly the records after N or a dropped marker naming the evicted range, a full queue drops the oldest and reports how many with their range, sequence numbers never go backwards across pump batches, secret setting values are masked before a record leaves, a bad subscribe request names its fault, and a subscriber that stops reading changes the cost of 100000 log lines by no more than ten percent against one that reads, with the cost of having no subscriber at all recorded beside them.
 */

#include "LogRedaction.h"
#include "LogStream.h"
#include "LogStreamHub.h"
#include "LogTestHarness.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    class RecordingSink final : public LogStreamSink
    {
    public:
        void Send(std::string text) override
        {
            std::lock_guard const lock(_mutex);
            _messages.push_back(nlohmann::json::parse(text));
        }

        void Close(std::string reason) override
        {
            std::lock_guard const lock(_mutex);
            _closeReason = std::move(reason);
            _closed = true;
        }

        std::vector<nlohmann::json> Messages() const
        {
            std::lock_guard const lock(_mutex);
            return _messages;
        }

        std::vector<nlohmann::json> OfType(std::string const& type) const
        {
            std::vector<nlohmann::json> out;
            for (nlohmann::json const& message : Messages())
                if (message.value("type", "") == type)
                    out.push_back(message);
            return out;
        }

        std::vector<uint64> Sequences() const
        {
            std::vector<uint64> out;
            for (nlohmann::json const& record : OfType("record"))
                out.push_back(record["sequence"].get<uint64>());
            return out;
        }

        bool IsClosed() const
        {
            std::lock_guard const lock(_mutex);
            return _closed;
        }

    private:
        mutable std::mutex _mutex;
        std::vector<nlohmann::json> _messages;
        std::string _closeReason;
        bool _closed = false;
    };

    class NeverDrainingSink final : public LogStreamSink
    {
    public:
        void Send(std::string) override {}
        void Close(std::string) override {}
    };

    LogMessage Record(uint64 sequence, LogLevel level, std::string category, std::string text)
    {
        LogMessage message;
        message.Sequence = sequence;
        message.Level = level;
        message.Category = std::move(category);
        message.Text = std::move(text);
        message.Time = std::chrono::system_clock::now();
        return message;
    }

    void PublishRange(LogStreamHub& hub, uint64 from, uint64 to, LogLevel level = LogLevel::Info)
    {
        for (uint64 sequence = from; sequence <= to; ++sequence)
            hub.Publish(Record(sequence, level, "test", "line " + std::to_string(sequence)));
    }

    std::vector<uint64> Range(uint64 from, uint64 to)
    {
        std::vector<uint64> out;
        for (uint64 sequence = from; sequence <= to; ++sequence)
            out.push_back(sequence);
        return out;
    }

    class LogStreamServiceTest : public testing::Test
    {
    protected:
        std::shared_ptr<LogStreamSession> Open(std::shared_ptr<RecordingSink> const& sink, LogStreamRequest const& request = {})
        {
            std::shared_ptr<LogStreamSession> const session = _service.Open(sink, request);
            while (_service.Pump() > 0)
            {
            }
            return session;
        }

        LogStreamHub _hub;
        LogStreamService _service{ _hub };
    };
}

TEST_F(LogStreamServiceTest, AWarnFilterPassesOnlyWarningsAndAbove)
{
    _hub.Publish(Record(1, LogLevel::Trace, "a", "trace"));
    _hub.Publish(Record(2, LogLevel::Debug, "a", "debug"));
    _hub.Publish(Record(3, LogLevel::Info, "a", "info"));
    _hub.Publish(Record(4, LogLevel::Warn, "a", "warn"));
    _hub.Publish(Record(5, LogLevel::Error, "a", "error"));
    _hub.Publish(Record(6, LogLevel::Fatal, "a", "fatal"));

    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.MinLevel = LogLevel::Warn;
    Open(sink, request);
    _hub.Publish(Record(7, LogLevel::Info, "a", "later info"));
    _hub.Publish(Record(8, LogLevel::Warn, "a", "later warn"));
    while (_service.Pump() > 0)
    {
    }

    EXPECT_EQ(sink->Sequences(), (std::vector<uint64>{ 4, 5, 6, 8 }));
    for (nlohmann::json const& record : sink->OfType("record"))
        EXPECT_NE(record["level"].get<std::string>(), "info") << record.dump();
    EXPECT_TRUE(sink->OfType("dropped").empty());
}

TEST_F(LogStreamServiceTest, ARecordCarriesWhereItWasWrittenAndWhatItWasWrittenFrom)
{
    LogMessage located = Record(1, LogLevel::Error, "a", "Listening on 127.0.0.1:12001 with 1 network thread(s)");
    located.Source = LogSource{ "src/server/Whatever.cpp", "Listen", 42 };
    located.Template = "Listening on {} with {} network thread(s)";
    LogMessage plain = Record(2, LogLevel::Error, "a", "written as text");

    _hub.Publish(std::move(located));
    _hub.Publish(std::move(plain));
    auto const sink = std::make_shared<RecordingSink>();
    Open(sink, LogStreamRequest{});
    while (_service.Pump() > 0)
    {
    }

    std::vector<nlohmann::json> const records = sink->OfType("record");
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0]["source"]["file"].get<std::string>(), "src/server/Whatever.cpp");
    EXPECT_EQ(records[0]["source"]["line"].get<uint32>(), 42u);
    EXPECT_EQ(records[0]["source"]["function"].get<std::string>(), "Listen");
    EXPECT_EQ(records[0]["template"].get<std::string>(), "Listening on {} with {} network thread(s)");
    EXPECT_TRUE(records[1]["source"].is_null()) << records[1].dump();
    EXPECT_EQ(records[1]["template"].get<std::string>(), "");
    for (nlohmann::json const& record : records)
        for (char const* field : { "sequence", "time", "epoch_ms", "level", "category", "message" })
            EXPECT_TRUE(record.contains(field)) << field << " " << record.dump();
}

TEST_F(LogStreamServiceTest, ACategoryFilterPassesOnlyThoseCategories)
{
    _hub.Publish(Record(1, LogLevel::Info, "server.admin", "admin"));
    _hub.Publish(Record(2, LogLevel::Info, "commands.console", "console"));
    _hub.Publish(Record(3, LogLevel::Info, "server", "server"));

    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.Categories = { "commands.console" };
    Open(sink, request);

    EXPECT_EQ(sink->Sequences(), (std::vector<uint64>{ 2 }));
}

TEST_F(LogStreamServiceTest, ANewSessionGetsTheHelloThenTheWholeBacklogInOrder)
{
    PublishRange(_hub, 1, 25);

    auto const sink = std::make_shared<RecordingSink>();
    Open(sink);

    std::vector<nlohmann::json> const messages = sink->Messages();
    ASSERT_FALSE(messages.empty());
    EXPECT_EQ(messages.front()["type"], "hello");
    EXPECT_EQ(messages.front()["latest"], 25u);
    EXPECT_EQ(messages.front()["oldest"], 1u);
    EXPECT_EQ(messages.front()["backlog"], 25u);
    EXPECT_EQ(sink->Sequences(), Range(1, 25));
    nlohmann::json const first = sink->OfType("record").front();
    EXPECT_EQ(first["message"], "line 1");
    EXPECT_EQ(first["category"], "test");
    EXPECT_EQ(first["level"], "info");
    EXPECT_TRUE(first.contains("time"));
    EXPECT_TRUE(first.contains("epoch_ms"));
}

TEST_F(LogStreamServiceTest, AResumeAfterNGetsExactlyTheRecordsAfterN)
{
    PublishRange(_hub, 1, 50);

    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.After = 30;
    std::shared_ptr<LogStreamSession> const session = Open(sink, request);

    EXPECT_EQ(sink->Sequences(), Range(31, 50));
    EXPECT_TRUE(sink->OfType("dropped").empty());
    EXPECT_EQ(session->GetLastSent(), 50u);
    EXPECT_EQ(session->GetSentCount(), 20u);
}

TEST_F(LogStreamServiceTest, AResumeAfterAnEvictedSequenceGetsADroppedMarkerNamingTheMissedRange)
{
    _hub.SetBacklogCapacity(10);
    PublishRange(_hub, 1, 50);

    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.After = 20;
    Open(sink, request);

    std::vector<nlohmann::json> const messages = sink->Messages();
    ASSERT_GE(messages.size(), 2u);
    EXPECT_EQ(messages[0]["type"], "hello");
    EXPECT_EQ(messages[0]["oldest"], 41u);
    EXPECT_EQ(messages[1]["type"], "dropped");
    EXPECT_EQ(messages[1]["from"], 21u);
    EXPECT_EQ(messages[1]["to"], 40u);
    EXPECT_EQ(messages[1]["count"], 20u);
    EXPECT_EQ(sink->Sequences(), Range(41, 50));
}

TEST_F(LogStreamServiceTest, AResumeAtTheLatestSequenceGetsNothingButTheHello)
{
    PublishRange(_hub, 1, 10);

    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.After = 10;
    Open(sink, request);

    EXPECT_EQ(sink->Messages().size(), 1u);
    EXPECT_TRUE(sink->Sequences().empty());
}

TEST_F(LogStreamServiceTest, AFullQueueDropsTheOldestAndReportsHowManyWithTheirRange)
{
    auto const sink = std::make_shared<RecordingSink>();
    LogStreamRequest request;
    request.QueueCapacity = 5;
    std::shared_ptr<LogStreamSession> const session = Open(sink, request);

    PublishRange(_hub, 1, 12);
    while (_service.Pump() > 0)
    {
    }

    std::vector<nlohmann::json> const dropped = sink->OfType("dropped");
    ASSERT_EQ(dropped.size(), 1u);
    EXPECT_EQ(dropped[0]["from"], 1u);
    EXPECT_EQ(dropped[0]["to"], 7u);
    EXPECT_EQ(dropped[0]["count"], 7u);
    EXPECT_EQ(sink->Sequences(), Range(8, 12));
    EXPECT_EQ(session->GetDroppedCount(), 7u);

    std::vector<nlohmann::json> const messages = sink->Messages();
    std::size_t const markerAt = static_cast<std::size_t>(std::find_if(messages.begin(), messages.end(), [](nlohmann::json const& message) { return message["type"] == "dropped"; }) - messages.begin());
    std::size_t const firstRecordAt = static_cast<std::size_t>(std::find_if(messages.begin(), messages.end(), [](nlohmann::json const& message) { return message["type"] == "record"; }) - messages.begin());
    EXPECT_LT(markerAt, firstRecordAt);
}

TEST_F(LogStreamServiceTest, SequenceNumbersNeverGoBackwardsAcrossPumpBatches)
{
    auto const sink = std::make_shared<RecordingSink>();
    Open(sink);

    uint64 next = 1;
    for (int round = 0; round < 20; ++round)
    {
        PublishRange(_hub, next, next + 99);
        next += 100;
        _service.Pump(7);
    }
    while (_service.Pump(7) > 0)
    {
    }

    std::vector<uint64> const sequences = sink->Sequences();
    EXPECT_EQ(sequences, Range(1, 2000));
    EXPECT_TRUE(std::is_sorted(sequences.begin(), sequences.end()));
}

TEST_F(LogStreamServiceTest, SecretSettingValuesAreMaskedBeforeARecordLeaves)
{
    _hub.Publish(Record(1, LogLevel::Info, "config", "Admin.Token = 0123456789abcdef0123456789abcdef"));
    _hub.Publish(Record(2, LogLevel::Info, "config", "LoginDatabaseInfo = \"127.0.0.1;3307;ambrose;hunter2;ambrose_login\""));
    _hub.Publish(Record(3, LogLevel::Info, "config", LogRedaction::DescribeSettingChange("Admin.Token", "fedcba9876543210fedcba9876543210", "console")));
    _hub.Publish(Record(4, LogLevel::Info, "config", "BindIP = 127.0.0.1 stays visible"));

    auto const sink = std::make_shared<RecordingSink>();
    Open(sink);

    std::vector<nlohmann::json> const records = sink->OfType("record");
    ASSERT_EQ(records.size(), 4u);
    for (nlohmann::json const& record : records)
    {
        std::string const text = record["message"].get<std::string>();
        EXPECT_EQ(text.find("0123456789abcdef"), std::string::npos) << text;
        EXPECT_EQ(text.find("hunter2"), std::string::npos) << text;
        EXPECT_EQ(text.find("fedcba9876543210"), std::string::npos) << text;
    }
    EXPECT_EQ(records[0]["message"], "Admin.Token = ***");
    EXPECT_EQ(records[1]["message"], "LoginDatabaseInfo = \"127.0.0.1;3307;ambrose;***;ambrose_login\"");
    EXPECT_EQ(records[2]["message"], "Setting Admin.Token changed to *** from console");
    EXPECT_EQ(records[3]["message"], "BindIP = 127.0.0.1 stays visible");
}

TEST_F(LogStreamServiceTest, ABadSubscribeRequestNamesItsFault)
{
    std::string error;
    EXPECT_FALSE(LogStreamService::ParseRequest("not json", error).has_value());
    EXPECT_NE(error.find("JSON object"), std::string::npos) << error;
    EXPECT_FALSE(LogStreamService::ParseRequest("[1,2]", error).has_value());
    EXPECT_FALSE(LogStreamService::ParseRequest("{\"level\":\"loud\"}", error).has_value());
    EXPECT_NE(error.find("level"), std::string::npos) << error;
    EXPECT_FALSE(LogStreamService::ParseRequest("{\"categories\":\"server\"}", error).has_value());
    EXPECT_NE(error.find("categories"), std::string::npos) << error;
    EXPECT_FALSE(LogStreamService::ParseRequest("{\"after\":-1}", error).has_value());
    EXPECT_NE(error.find("after"), std::string::npos) << error;

    std::optional<LogStreamRequest> const request = LogStreamService::ParseRequest("{\"level\":\"warn\",\"categories\":[\"server\",\"commands.console\"],\"after\":42}", error);
    ASSERT_TRUE(request.has_value()) << error;
    EXPECT_EQ(request->MinLevel, LogLevel::Warn);
    EXPECT_EQ(request->Categories, (std::vector<std::string>{ "server", "commands.console" }));
    EXPECT_EQ(request->After, 42u);
    EXPECT_TRUE(LogStreamService::ParseRequest("{}", error).has_value());
}

TEST_F(LogStreamServiceTest, ClosingASessionUnsubscribesItAndClosesItsSink)
{
    auto const sink = std::make_shared<RecordingSink>();
    std::shared_ptr<LogStreamSession> const session = Open(sink);
    EXPECT_EQ(_service.GetSessionCount(), 1u);
    EXPECT_EQ(_hub.GetSubscriberCount(), 1u);

    _service.Close(session);

    EXPECT_TRUE(sink->IsClosed());
    EXPECT_TRUE(session->IsClosed());
    EXPECT_EQ(_service.GetSessionCount(), 0u);
    EXPECT_EQ(_hub.GetSubscriberCount(), 0u);
}

TEST_F(LogStreamServiceTest, ThePumpThreadDeliversWithoutAnyManualPump)
{
    _service.Start();
    auto const sink = std::make_shared<RecordingSink>();
    _service.Open(sink, {});
    PublishRange(_hub, 1, 100);

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (sink->Sequences().size() < 100 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_EQ(sink->Sequences(), Range(1, 100));
    _service.Stop();
    EXPECT_TRUE(sink->IsClosed());
}

TEST(LogStreamCost, ASubscriberThatStopsReadingChangesLoggingCostByNoMoreThanTenPercent)
{
    constexpr int Rounds = 20;
    constexpr int LinesPerRound = 5000;
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Stream = 3,1,0,1000\nLogger.root = 1,Stream\n");
    Log& log = harness.GetLog();

    auto const measure = [&]
    {
        auto const start = std::chrono::steady_clock::now();
        for (int line = 0; line < LinesPerRound; ++line)
            AMBROSE_LOG(log, LogLevel::Info, "cost", "line {} of the cost measurement", line);
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    };
    auto const measureWithSubscriber = [&](bool reading)
    {
        LogStreamService service(log.GetStreamHub());
        if (reading)
            service.Start();
        LogStreamRequest request;
        request.QueueCapacity = 100;
        std::shared_ptr<LogStreamSession> const session = service.Open(std::make_shared<NeverDrainingSink>(), request);
        double const seconds = measure();
        service.Close(session);
        return seconds;
    };

    double alone = 1e9;
    double reading = 1e9;
    double stalled = 1e9;
    for (int round = 0; round < Rounds; ++round)
    {
        alone = std::min(alone, measure());
        reading = std::min(reading, measureWithSubscriber(true));
        stalled = std::min(stalled, measureWithSubscriber(false));
    }

    RecordProperty("best_round_alone_us", static_cast<int>(alone * 1e6));
    RecordProperty("best_round_reading_subscriber_us", static_cast<int>(reading * 1e6));
    RecordProperty("best_round_stalled_subscriber_us", static_cast<int>(stalled * 1e6));
    EXPECT_LE(stalled, reading * 1.10) << "best round alone " << alone << " s, with a reading subscriber " << reading << " s, with one that stopped reading " << stalled << " s, over " << Rounds * LinesPerRound << " lines each";
}

TEST_F(LogStreamServiceTest, TheBacklogIsReadableOverHttpAfterASequenceNumber)
{
    PublishRange(_hub, 1, 20);

    nlohmann::json const all = nlohmann::json::parse(LogStreamService::BacklogJson(_hub, 0, 500), nullptr, false);
    ASSERT_TRUE(all.is_object());
    EXPECT_EQ(all["schema"], 1);
    EXPECT_EQ(all["oldest"], 1u);
    EXPECT_EQ(all["latest"], 20u);
    EXPECT_EQ(all["records"].size(), 20u);
    EXPECT_TRUE(all["dropped"].is_null());

    nlohmann::json const rest = nlohmann::json::parse(LogStreamService::BacklogJson(_hub, 15, 500), nullptr, false);
    ASSERT_EQ(rest["records"].size(), 5u) << "only what came after 15";
    EXPECT_EQ(rest["records"][0]["sequence"], 16u);

    nlohmann::json const page = nlohmann::json::parse(LogStreamService::BacklogJson(_hub, 0, 4), nullptr, false);
    EXPECT_EQ(page["records"].size(), 4u) << "a page is capped and the caller comes back for more";
    EXPECT_EQ(page["records"][0]["sequence"], 1u);
}

TEST_F(LogStreamServiceTest, TheHttpReadSaysWhatItCouldNotShowAndMatchesTheSocketsRecord)
{
    _hub.SetBacklogCapacity(10);
    PublishRange(_hub, 1, 30);

    nlohmann::json const missed = nlohmann::json::parse(LogStreamService::BacklogJson(_hub, 5, 500), nullptr, false);
    ASSERT_FALSE(missed["dropped"].is_null()) << "the reader asked for lines the backlog no longer holds";
    EXPECT_EQ(missed["dropped"]["from"], 6u);
    EXPECT_EQ(missed["dropped"]["count"].get<uint64>(), missed["dropped"]["to"].get<uint64>() - 5u);

    LogMessage one = Record(31, LogLevel::Error, "a", "the same line both ways");
    one.Template = "the same line both ways";
    _hub.Publish(one);

    nlohmann::json const overHttp = nlohmann::json::parse(LogStreamService::BacklogJson(_hub, 30, 500), nullptr, false)["records"][0];
    nlohmann::json const overSocket = nlohmann::json::parse(LogStreamService::EncodeRecord(one), nullptr, false);
    for (char const* field : { "sequence", "level", "category", "message", "template", "source" })
        EXPECT_EQ(overHttp[field], overSocket[field]) << field << " differs between the two ways of reading the same record";
}
