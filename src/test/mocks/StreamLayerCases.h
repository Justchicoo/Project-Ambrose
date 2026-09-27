/*
 * Project Ambrose by Imjustchico
 * The stream layer's own cases, written once and run against every feed built on it, the live log and the admin events alike: a new session gets the hello and the whole backlog in order, a resume after N gets exactly what came after N, a resume after a sequence the backlog no longer holds gets a dropped marker naming the missed range, a resume at the latest sequence gets nothing but the hello, a full queue drops the oldest and names how many and which, sequence numbers never go backwards across pump batches, and the backlog read over HTTP after a sequence number says what it could no longer show. A feed passes by naming its hub, service, request, the type its records are sent as and how to publish a run of them.
 */

#ifndef AMBROSE_STREAMLAYERCASES_H
#define AMBROSE_STREAMLAYERCASES_H

#include "StreamService.h"
#include "Types.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class RecordingStreamSink final : public StreamSink
{
public:
    explicit RecordingStreamSink(std::string recordType = "record") : _recordType(std::move(recordType))
    {
    }

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
        for (nlohmann::json const& record : OfType(_recordType))
            out.push_back(record["sequence"].get<uint64>());
        return out;
    }

    bool IsClosed() const
    {
        std::lock_guard const lock(_mutex);
        return _closed;
    }

private:
    std::string _recordType;
    mutable std::mutex _mutex;
    std::vector<nlohmann::json> _messages;
    std::string _closeReason;
    bool _closed = false;
};

inline std::vector<uint64> SequenceRange(uint64 from, uint64 to)
{
    std::vector<uint64> out;
    for (uint64 sequence = from; sequence <= to; ++sequence)
        out.push_back(sequence);
    return out;
}

template<class Source>
struct StreamLayerCases
{
    using Hub = typename Source::Hub;
    using Service = typename Source::Service;
    using Request = typename Source::Request;

    static std::shared_ptr<RecordingStreamSink> Sink()
    {
        return std::make_shared<RecordingStreamSink>(std::string(Source::RecordType));
    }

    static auto Open(Service& service, std::shared_ptr<RecordingStreamSink> const& sink, Request const& request = {})
    {
        auto const session = service.Open(sink, request);
        while (service.Pump() > 0)
        {
        }
        return session;
    }

    static void NewSessionGetsHelloThenBacklogInOrder(Hub& hub, Service& service)
    {
        Source::PublishRange(hub, 1, 25);
        auto const sink = Sink();
        Open(service, sink);

        std::vector<nlohmann::json> const messages = sink->Messages();
        ASSERT_FALSE(messages.empty());
        EXPECT_EQ(messages.front()["type"], "hello");
        EXPECT_EQ(messages.front()["latest"], 25u);
        EXPECT_EQ(messages.front()["oldest"], 1u);
        EXPECT_EQ(messages.front()["backlog"], 25u);
        EXPECT_EQ(sink->Sequences(), SequenceRange(1, 25));
        nlohmann::json const first = sink->OfType(std::string(Source::RecordType)).front();
        EXPECT_TRUE(first.contains("time"));
        EXPECT_TRUE(first.contains("epoch_ms"));
    }

    static void ResumeAfterN(Hub& hub, Service& service)
    {
        Source::PublishRange(hub, 1, 50);
        auto const sink = Sink();
        Request request;
        request.After = 30;
        auto const session = Open(service, sink, request);

        EXPECT_EQ(sink->Sequences(), SequenceRange(31, 50));
        EXPECT_TRUE(sink->OfType("dropped").empty());
        EXPECT_EQ(session->GetLastSent(), 50u);
        EXPECT_EQ(session->GetSentCount(), 20u);
    }

    static void ResumeAfterEvicted(Hub& hub, Service& service)
    {
        hub.SetBacklogCapacity(10);
        Source::PublishRange(hub, 1, 50);
        auto const sink = Sink();
        Request request;
        request.After = 20;
        Open(service, sink, request);

        std::vector<nlohmann::json> const messages = sink->Messages();
        ASSERT_GE(messages.size(), 2u);
        EXPECT_EQ(messages[0]["type"], "hello");
        EXPECT_EQ(messages[0]["oldest"], 41u);
        EXPECT_EQ(messages[1]["type"], "dropped");
        EXPECT_EQ(messages[1]["from"], 21u);
        EXPECT_EQ(messages[1]["to"], 40u);
        EXPECT_EQ(messages[1]["count"], 20u);
        EXPECT_EQ(sink->Sequences(), SequenceRange(41, 50));
    }

    static void ResumeAtLatest(Hub& hub, Service& service)
    {
        Source::PublishRange(hub, 1, 10);
        auto const sink = Sink();
        Request request;
        request.After = 10;
        Open(service, sink, request);

        EXPECT_EQ(sink->Messages().size(), 1u);
        EXPECT_TRUE(sink->Sequences().empty());
    }

    static void FullQueueDrops(Hub& hub, Service& service)
    {
        auto const sink = Sink();
        Request request;
        request.QueueCapacity = 5;
        auto const session = Open(service, sink, request);

        Source::PublishRange(hub, 1, 12);
        while (service.Pump() > 0)
        {
        }

        std::vector<nlohmann::json> const dropped = sink->OfType("dropped");
        ASSERT_EQ(dropped.size(), 1u);
        EXPECT_EQ(dropped[0]["from"], 1u);
        EXPECT_EQ(dropped[0]["to"], 7u);
        EXPECT_EQ(dropped[0]["count"], 7u);
        EXPECT_EQ(sink->Sequences(), SequenceRange(8, 12));
        EXPECT_EQ(session->GetDroppedCount(), 7u);

        std::vector<nlohmann::json> const messages = sink->Messages();
        std::string const recordType(Source::RecordType);
        std::size_t const markerAt = static_cast<std::size_t>(std::find_if(messages.begin(), messages.end(), [](nlohmann::json const& message) { return message["type"] == "dropped"; }) - messages.begin());
        std::size_t const firstRecordAt = static_cast<std::size_t>(std::find_if(messages.begin(), messages.end(), [&recordType](nlohmann::json const& message) { return message["type"] == recordType; }) - messages.begin());
        EXPECT_LT(markerAt, firstRecordAt);
    }

    static void SequenceNeverBackwards(Hub& hub, Service& service)
    {
        auto const sink = Sink();
        Open(service, sink);

        uint64 next = 1;
        for (int round = 0; round < 20; ++round)
        {
            Source::PublishRange(hub, next, next + 99);
            next += 100;
            service.Pump(7);
        }
        while (service.Pump(7) > 0)
        {
        }

        std::vector<uint64> const sequences = sink->Sequences();
        EXPECT_EQ(sequences, SequenceRange(1, 2000));
        EXPECT_TRUE(std::is_sorted(sequences.begin(), sequences.end()));
    }

    static void BacklogOverHttp(Hub& hub)
    {
        hub.SetBacklogCapacity(10);
        Source::PublishRange(hub, 1, 30);

        nlohmann::json const all = nlohmann::json::parse(Service::BacklogJson(hub, 20, 500), nullptr, false);
        ASSERT_TRUE(all.is_object());
        EXPECT_EQ(all["schema"], 1);
        EXPECT_EQ(all["oldest"], 21u);
        EXPECT_EQ(all["latest"], 30u);
        EXPECT_EQ(all["records"].size(), 10u);
        EXPECT_TRUE(all["dropped"].is_null());

        nlohmann::json const missed = nlohmann::json::parse(Service::BacklogJson(hub, 5, 500), nullptr, false);
        ASSERT_FALSE(missed["dropped"].is_null()) << "the reader asked for records the backlog no longer holds";
        EXPECT_EQ(missed["dropped"]["from"], 6u);
        EXPECT_EQ(missed["dropped"]["to"], 20u);
        EXPECT_EQ(missed["dropped"]["count"], 15u);

        nlohmann::json const page = nlohmann::json::parse(Service::BacklogJson(hub, 20, 4), nullptr, false);
        ASSERT_EQ(page["records"].size(), 4u) << "a page is capped and the caller comes back for more";
        EXPECT_EQ(page["records"][0]["sequence"], 21u);
    }
};

#endif
