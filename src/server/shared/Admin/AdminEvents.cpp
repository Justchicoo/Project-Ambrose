/*
 * Project Ambrose by Imjustchico
 * Holds the one compiled copy of the stream layer over admin events, matches kinds at dots, reads a subscribe message naming kinds and a sequence to resume after, writes each event as JSON with its data carried as the object it was announced with, and builds the data of a setting change, masked through the registry, and of a reload's outcome.
 */

#include "AdminEvents.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

template class StreamHub<AdminEvent, AdminEventFilter>;
template class StreamSession<AdminEventTraits>;
template class StreamService<AdminEventTraits>;

bool AdminEventFilter::Matches(AdminEvent const& event) const noexcept
{
    if (Kinds.empty())
        return true;
    std::string_view const kind = event.Kind;
    for (std::string const& wanted : Kinds)
        if (kind == wanted || (kind.size() > wanted.size() && kind.starts_with(wanted) && kind[wanted.size()] == '.'))
            return true;
    return false;
}

AdminEventFilter AdminEventTraits::FilterOf(AdminEventRequest const& request)
{
    return AdminEventFilter{ request.Kinds };
}

std::string AdminEventTraits::Encode(AdminEvent const& event)
{
    nlohmann::json body;
    body["type"] = "event";
    body["sequence"] = event.Sequence;
    body["time"] = std::string(LogTimestamp::FormatPrefix(event.Time, true));
    body["epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(event.Time.time_since_epoch()).count();
    body["kind"] = event.Kind;
    body["subject"] = event.Subject;
    nlohmann::json data = nlohmann::json::parse(event.Data, nullptr, false);
    body["data"] = data.is_discarded() ? nlohmann::json::object() : std::move(data);
    return body.dump();
}

std::optional<AdminEventRequest> AdminEventTraits::Parse(std::string const& text, std::string& error)
{
    nlohmann::json const document = nlohmann::json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "the subscribe message must be a JSON object";
        return std::nullopt;
    }
    AdminEventRequest request;
    for (auto const& [key, value] : document.items())
    {
        if (key == "kinds")
        {
            if (!value.is_array())
            {
                error = "kinds must be a list of event kinds, such as setting or reload.result";
                return std::nullopt;
            }
            for (nlohmann::json const& entry : value)
            {
                if (!entry.is_string() || entry.get_ref<std::string const&>().empty())
                {
                    error = "kinds must be a list of event kinds, such as setting or reload.result";
                    return std::nullopt;
                }
                request.Kinds.push_back(entry.get<std::string>());
            }
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
            error = "a subscribe message takes only kinds and after, not " + Ambrose::ForLog(key, 64);
            return std::nullopt;
        }
    }
    return request;
}

uint64 AdminEventHub::Announce(std::string kind, std::string subject, std::string data)
{
    std::lock_guard const lock(_stampMutex);
    AdminEvent event;
    event.Sequence = ++_sequence;
    event.Kind = std::move(kind);
    event.Subject = std::move(subject);
    event.Time = std::chrono::system_clock::now();
    event.Data = std::move(data);
    Publish(event);
    return event.Sequence;
}

uint64 AdminEventHub::GetLatestSequence() const
{
    std::lock_guard const lock(_stampMutex);
    return _sequence;
}

AdminEventService::AdminEventService(AdminEventHub& hub) : StreamService<AdminEventTraits>(hub)
{
}

std::string AdminEventData::SettingChanged(Settings const& settings, SettingChange const& change)
{
    nlohmann::json data;
    data["old"] = settings.Shown(change.Key, change.OldValue);
    data["new"] = settings.Shown(change.Key, change.NewValue);
    data["layer"] = nullptr;
    data["origin"] = nullptr;
    data["visibility"] = settings.IsSecret(change.Key) ? "secret" : "normal";
    if (std::optional<SettingView> const view = settings.Describe(change.Key))
    {
        data["layer"] = Settings::LayerCode(view->Layer);
        data["origin"] = view->Origin;
    }
    data["who"] = change.Author.Who;
    data["account_id"] = change.Author.AccountId;
    data["source"] = change.Author.Source;
    data["reason"] = change.Reason;
    data["epoch_seconds"] = change.EpochSeconds;
    return data.dump();
}

std::string AdminEventData::ReloadResult(ReloadOutcome const& outcome)
{
    nlohmann::json data;
    data["generation"] = outcome.Generation;
    data["ok"] = outcome.Ok;
    data["errors"] = outcome.Errors;
    data["finished_ms"] = outcome.FinishedEpochMs;
    return data.dump();
}
