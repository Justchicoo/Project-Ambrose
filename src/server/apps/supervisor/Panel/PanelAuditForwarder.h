/*
 * Project Ambrose by Imjustchico
 * Drains the panel audit outbox to an optional HTTPS collector, retaining unacknowledged rows and retrying with capped backoff.
 */

#ifndef AMBROSE_PANELAUDITFORWARDER_H
#define AMBROSE_PANELAUDITFORWARDER_H

#include "Types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

class PanelStore;

class PanelAuditForwarder
{
public:
    using Report = std::function<void(std::string_view)>;
    using Sender = std::function<bool(std::string_view url, std::string_view token, std::string_view payload, std::string& error)>;

    PanelAuditForwarder() = default;
    ~PanelAuditForwarder();

    PanelAuditForwarder(PanelAuditForwarder const&) = delete;
    PanelAuditForwarder& operator=(PanelAuditForwarder const&) = delete;

    bool Start(PanelStore& store, std::mutex& storeMutex, std::string url, std::string token, Report report, std::string& error, Sender sender = {});
    void Stop();
    void Wake();

private:
    void Run();

    std::mutex _mutex;
    std::condition_variable _wake;
    std::thread _thread;
    PanelStore* _store = nullptr;
    std::mutex* _storeMutex = nullptr;
    std::string _url;
    std::string _token;
    Report _report;
    Sender _sender;
    bool _stopping = false;
    bool _backoffActive = false;
    std::chrono::steady_clock::time_point _retryAt{};
    int64 _attempt = 0;
};

#endif
