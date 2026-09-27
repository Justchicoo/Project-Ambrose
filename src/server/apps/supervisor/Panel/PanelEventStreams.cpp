/*
 * Project Ambrose by Imjustchico
 * Holds the one compiled copy of the stream layer over panel events, matches a record against the app a session follows and the apps its caller may see, writes each record as the socket's envelope with its sequence and time, reads a request naming a stream, an app and a sequence to resume after with anything else refused, stamps sequences in the hub, refuses to publish a message the catalog does not mark as sent on that stream, and builds one hub and pump per stream the catalog marks served.
 */

#include "PanelEventStreams.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "PanelEventCatalog.h"
#include "PanelEventFrame.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <array>
#include <stdexcept>
#include <utility>

template class StreamHub<PanelEvent, PanelEventFilter>;
template class StreamSession<PanelEventTraits>;
template class StreamService<PanelEventTraits>;

bool PanelEventFilter::Matches(PanelEvent const& event) const noexcept
{
    if (App && *App != event.App)
        return false;
    if (Visible && !event.App.empty() && !Visible->contains(event.App))
        return false;
    return true;
}

PanelEventFilter PanelEventTraits::FilterOf(PanelEventRequest const& request)
{
    return PanelEventFilter{ request.App, request.Visible };
}

StreamOverflow PanelEventTraits::OverflowOf(PanelEventRequest const& request)
{
    return request.Overflow;
}

std::string PanelEventTraits::Encode(PanelEvent const& event)
{
    return PanelEventFrame::Write(event.Type, std::nullopt, event.App, event.Sequence, event.TimeMs, event.Data);
}

std::optional<PanelEventRequest> PanelEventTraits::Parse(std::string const& text, std::string& error)
{
    nlohmann::json const document = nlohmann::json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "a stream request must be a JSON object";
        return std::nullopt;
    }
    PanelEventRequest request;
    for (auto entry = document.begin(); entry != document.end(); ++entry)
    {
        std::string const& key = entry.key();
        nlohmann::json const& value = entry.value();
        if (key == "stream")
        {
            if (!value.is_string() || !PanelEventCatalog::FindStream(value.get_ref<std::string const&>()))
            {
                error = "stream must name one of the panel's streams";
                return std::nullopt;
            }
            request.Stream = value.get<std::string>();
        }
        else if (key == "app")
        {
            if (!value.is_string() || value.get_ref<std::string const&>().empty())
            {
                error = "app must name an app";
                return std::nullopt;
            }
            request.App = value.get<std::string>();
        }
        else if (key == "after")
        {
            if (!value.is_number_unsigned())
            {
                error = "after must be a sequence number";
                return std::nullopt;
            }
            request.After = value.get<uint64>();
        }
        else
        {
            error = "a stream request takes only stream, app and after, not " + Ambrose::ForLog(key, 64);
            return std::nullopt;
        }
    }
    if (request.Stream.empty())
    {
        error = "a stream request names its stream";
        return std::nullopt;
    }
    return request;
}

uint64 PanelEventHub::Announce(std::string type, std::string app, std::string data, std::string key)
{
    std::lock_guard const lock(_stampMutex);
    PanelEvent event;
    event.Sequence = ++_sequence;
    event.Type = std::move(type);
    event.App = std::move(app);
    event.TimeMs = PanelEventFrame::NowMs();
    event.Data = data.empty() ? std::string("{}") : std::move(data);
    event.Key = std::move(key);
    uint64 const sequence = event.Sequence;
    Publish(std::make_shared<PanelEvent const>(std::move(event)));
    return sequence;
}

uint64 PanelEventHub::GetLatestSequence() const
{
    std::lock_guard const lock(_stampMutex);
    return _sequence;
}

PanelEventService::PanelEventService(PanelEventHub& hub) : StreamService<PanelEventTraits>(hub)
{
}

PanelEventStreams::PanelEventStreams()
{
    std::array<uint8, 12> const bytes = Ambrose::Crypto::GetRandomArray<12>();
    _instance = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    for (PanelEventStream const& stream : PanelEventCatalog::Streams())
    {
        if (!stream.Served)
            continue;
        auto records = std::make_unique<PanelEventHub>();
        auto pump = std::make_unique<PanelEventService>(*records);
        _feeds.emplace(std::string(stream.Name), Feed{ std::move(records), std::move(pump), stream.Overflow });
    }
}

void PanelEventStreams::Start()
{
    for (auto& entry : _feeds)
        entry.second.Pump->Start();
}

void PanelEventStreams::Stop()
{
    for (auto& entry : _feeds)
        entry.second.Pump->Stop();
}

bool PanelEventStreams::Serves(std::string_view stream) const
{
    return _feeds.find(stream) != _feeds.end();
}

std::vector<std::string> PanelEventStreams::Served() const
{
    std::vector<std::string> names;
    names.reserve(_feeds.size());
    for (auto const& entry : _feeds)
        names.push_back(entry.first);
    return names;
}

PanelEventHub* PanelEventStreams::Hub(std::string_view stream)
{
    auto const found = _feeds.find(stream);
    return found == _feeds.end() ? nullptr : found->second.Records.get();
}

PanelEventService* PanelEventStreams::Service(std::string_view stream)
{
    auto const found = _feeds.find(stream);
    return found == _feeds.end() ? nullptr : found->second.Pump.get();
}

uint64 PanelEventStreams::Publish(std::string_view stream, std::string type, std::string app, std::string data, std::string key)
{
    if (!PanelEventCatalog::IsSent(type, stream))
        throw std::invalid_argument(fmt::format("{} is not a message the catalog marks as sent on the {} stream, so no page handles it", Ambrose::ForLog(type, 64), Ambrose::ForLog(stream, 64)));
    PanelEventHub* const hub = Hub(stream);
    return hub ? hub->Announce(std::move(type), std::move(app), std::move(data), std::move(key)) : 0;
}

std::shared_ptr<PanelEventSession> PanelEventStreams::Open(std::shared_ptr<StreamSink> sink, PanelEventRequest request)
{
    auto const found = _feeds.find(request.Stream);
    if (found == _feeds.end())
        return nullptr;
    request.Overflow = found->second.Overflow;
    return found->second.Pump->Open(std::move(sink), request);
}

void PanelEventStreams::Close(std::string_view stream, std::shared_ptr<PanelEventSession> const& session)
{
    if (!session)
        return;
    if (PanelEventService* const service = Service(stream))
        service->Close(session);
    else
        session->Close();
}
