/*
 * Project Ambrose by Imjustchico
 * Sends queued audit batches only over hostname-verified HTTPS, acknowledges successful responses, and schedules failed batches with capped exponential backoff; each batch goes through the shared admin client with peer verification, and any answer outside 2xx is a failure.
 */

#include "PanelAuditForwarder.h"
#include "AdminClient.h"
#include "PanelStore.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    struct Endpoint
    {
        std::string Host;
        uint16 Port = 443;
        std::string Target;
    };

    bool ParseEndpoint(std::string_view url, Endpoint& endpoint, std::string& error)
    {
        constexpr std::string_view Scheme = "https://";
        if (!url.starts_with(Scheme))
        {
            error = "the audit collector URL must use HTTPS";
            return false;
        }
        url.remove_prefix(Scheme.size());
        std::size_t const pathStart = url.find('/');
        std::string_view const authority = url.substr(0, pathStart);
        std::string_view const target = pathStart == std::string_view::npos ? std::string_view("/") : url.substr(pathStart);
        if (authority.empty() || authority.find_first_of("@?#\r\n") != std::string_view::npos
            || target.find_first_of("\r\n#") != std::string_view::npos)
        {
            error = "the audit collector URL has an invalid authority or path";
            return false;
        }

        std::string_view host = authority;
        std::string_view port = "443";
        if (authority.front() == '[')
        {
            std::size_t const close = authority.find(']');
            if (close == std::string_view::npos)
            {
                error = "the audit collector IPv6 host needs closing brackets";
                return false;
            }
            host = authority.substr(1, close - 1);
            std::string_view const suffix = authority.substr(close + 1);
            if (!suffix.empty())
            {
                if (suffix.front() != ':')
                {
                    error = "the audit collector URL has an invalid port";
                    return false;
                }
                port = suffix.substr(1);
            }
        }
        else if (std::size_t const colon = authority.rfind(':'); colon != std::string_view::npos)
        {
            host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
        }
        uint16 portNumber = 0;
        auto const [end, parsed] = std::from_chars(port.data(), port.data() + port.size(), portNumber);
        if (host.empty() || port.empty() || parsed != std::errc() || end != port.data() + port.size() || portNumber == 0)
        {
            error = "the audit collector URL needs a host and a port from 1 to 65535";
            return false;
        }
        endpoint = { std::string(host), portNumber, std::string(target) };
        return true;
    }

    bool SendBatch(Endpoint const& endpoint, std::string_view token, std::string const& body, std::string& error)
    {
        AdminClient const client(endpoint.Host, endpoint.Port, std::string(token), true, true);
        AdminClientRequest request;
        request.Method = "POST";
        request.Path = endpoint.Target;
        request.Body = body;
        AdminClientResponse const response = client.Send(request, std::chrono::seconds(10));
        if (!response.Answered)
        {
            error = fmt::format("the audit collector gave no answer: {}", response.Error);
            return false;
        }
        if (response.Status < 200 || response.Status >= 300)
        {
            error = fmt::format("the audit collector refused the batch with HTTP {}", response.Status);
            return false;
        }
        return true;
    }

    struct QueuedEvent
    {
        std::string EventId;
        std::string Payload;
        int64 Attempts = 0;
    };
}

PanelAuditForwarder::~PanelAuditForwarder()
{
    Stop();
}

bool PanelAuditForwarder::Start(PanelStore& store, std::mutex& storeMutex, std::string url, std::string token, Report report, std::string& error, Sender sender)
{
    error.clear();
    Stop();
    if (url.empty())
    {
        if (!token.empty())
        {
            error = "Panel.AuditCollectorToken needs Panel.AuditCollectorUrl";
            return false;
        }
        return true;
    }
    Endpoint endpoint;
    if (!ParseEndpoint(url, endpoint, error))
        return false;
    if (token.empty() || token.find_first_of("\r\n \t") != std::string::npos)
    {
        error = "Panel.AuditCollectorToken must be set and contain no line breaks when forwarding is enabled";
        return false;
    }
    {
        std::lock_guard const lock(_mutex);
        _store = &store;
        _storeMutex = &storeMutex;
        _url = std::move(url);
        _token = std::move(token);
        _report = std::move(report);
        _sender = std::move(sender);
        _stopping = false;
    }
    _thread = std::thread([this] { Run(); });
    return true;
}

void PanelAuditForwarder::Stop()
{
    {
        std::lock_guard const lock(_mutex);
        _stopping = true;
    }
    _wake.notify_all();
    if (_thread.joinable())
        _thread.join();
    std::lock_guard const lock(_mutex);
    _store = nullptr;
    _storeMutex = nullptr;
    _url.clear();
    _token.clear();
    _report = {};
    _sender = {};
    _backoffActive = false;
    _retryAt = {};
    _attempt = 0;
}

void PanelAuditForwarder::Wake()
{
    _wake.notify_all();
}

void PanelAuditForwarder::Run()
{
    while (true)
    {
        PanelStore* store = nullptr;
        std::mutex* storeMutex = nullptr;
        std::string url;
        std::string token;
        Report report;
        Sender sender;
        {
            std::lock_guard const lock(_mutex);
            if (_stopping)
                return;
            store = _store;
            storeMutex = _storeMutex;
            url = _url;
            token = _token;
            report = _report;
            sender = _sender;
        }

        std::vector<QueuedEvent> events;
        std::string error;
        bool waitForRetry = false;
        {
            std::lock_guard const lock(*storeMutex);
            std::optional<PanelStore::Statement> rows = store->Prepare(
                "SELECT event_id, payload, attempts, retry_epoch_ms FROM panel_audit_outbox "
                "WHERE delivered_epoch_ms IS NULL ORDER BY rowid LIMIT 25", error);
            if (rows)
            {
                while (rows->Step(error))
                {
                    events.push_back({ rows->Text(0), rows->Text(1), rows->Int64(2) });
                }
            }
        }
        if (!events.empty())
        {
            {
                std::lock_guard const lock(*storeMutex);
                std::optional<PanelStore::Statement> first = store->Prepare(
                    "SELECT retry_epoch_ms FROM panel_audit_outbox WHERE event_id = ? AND delivered_epoch_ms IS NULL", error);
                if (first)
                {
                    first->Bind(1, events.front().EventId);
                    if (!first->Step(error) || first->Int64(0) > PanelStore::NowEpochMs())
                        waitForRetry = true;
                }
                else
                    waitForRetry = true;
            }
        }
        if (!error.empty())
            waitForRetry = true;
        {
            std::lock_guard const lock(_mutex);
            if (_backoffActive && std::chrono::steady_clock::now() < _retryAt)
                waitForRetry = true;
        }
        if (!error.empty())
        {
            if (report)
                report(error);
        }
        else if (!events.empty() && !waitForRetry)
        {
            Endpoint endpoint;
            nlohmann::json payload;
            payload["events"] = nlohmann::json::array();
            for (QueuedEvent const& event : events)
            {
                nlohmann::json parsed = nlohmann::json::parse(event.Payload, nullptr, false);
                if (!parsed.is_object())
                {
                    error = "a queued audit event is not valid JSON";
                    break;
                }
                payload["events"].push_back(std::move(parsed));
            }
            if (error.empty())
            {
                std::string const batch = payload.dump();
                if (sender)
                {
                    if (!sender(url, token, batch, error) && error.empty())
                        error = "the audit collector did not acknowledge the batch";
                }
                else if (ParseEndpoint(url, endpoint, error))
                    SendBatch(endpoint, token, batch, error);
            }

            std::string const sendError = error;
            std::string databaseError;
            std::lock_guard const lock(*storeMutex);
            if (store->Begin(databaseError))
            {
                std::optional<PanelStore::Statement> update = sendError.empty()
                    ? store->Prepare("UPDATE panel_audit_outbox SET delivered_epoch_ms = ?, last_problem = '' WHERE event_id = ? AND delivered_epoch_ms IS NULL", databaseError)
                    : store->Prepare("UPDATE panel_audit_outbox SET attempts = attempts + 1, retry_epoch_ms = ?, last_problem = ? WHERE event_id = ? AND delivered_epoch_ms IS NULL", databaseError);
                if (update)
                {
                    for (QueuedEvent const& event : events)
                    {
                        update->Reset();
                        if (sendError.empty())
                        {
                            update->Bind(1, PanelStore::NowEpochMs());
                            update->Bind(2, event.EventId);
                        }
                        else
                        {
                            int64 const attempt = std::min<int64>(event.Attempts, 8);
                            int64 const delayMs = std::min<int64>(300'000, 1000LL << attempt);
                            update->Bind(1, PanelStore::NowEpochMs() + delayMs);
                            update->Bind(2, sendError);
                            update->Bind(3, event.EventId);
                        }
                        if (!update->Run(databaseError))
                            break;
                    }
                }
                if (databaseError.empty())
                    store->Commit(databaseError);
                else
                    store->Rollback();
            }
            if (!databaseError.empty() && report)
                report(databaseError);
            else if (!sendError.empty() && report)
                report(sendError);
            {
                std::lock_guard const stateLock(_mutex);
                _backoffActive = !sendError.empty() || !databaseError.empty();
                if (!_backoffActive)
                {
                    _attempt = 0;
                    _retryAt = {};
                }
                else
                {
                    int64 const attempt = std::min<int64>(_attempt++, 8);
                    _retryAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::min<int64>(300'000, 1000LL << attempt));
                }
            }
        }
        else if (!error.empty())
        {
            std::lock_guard const lock(_mutex);
            _backoffActive = true;
            int64 const attempt = std::min<int64>(_attempt++, 8);
            _retryAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::min<int64>(300'000, 1000LL << attempt));
        }

        std::unique_lock lock(_mutex);
        if (_stopping)
            return;
        _wake.wait_for(lock, std::chrono::seconds(1), [this] { return _stopping; });
    }
}
