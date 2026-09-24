/*
 * Project Ambrose by Imjustchico
 * Publishes routing generations, delivers records synchronously or through the worker, and manages reloads and shutdown.
 */

#include "Log.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "AppenderPending.h"
#include "AppenderStream.h"
#include "ConsoleWriter.h"
#include "LogFileRegistry.h"
#include "LogRouting.h"
#include "LogStreamHub.h"
#include "LogWorker.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <system_error>
#include <thread>
#include <utility>

struct Log::State
{
    std::shared_ptr<LogRouting const> Routing;
    std::shared_ptr<LogWorker> Worker;
};

struct Log::FlushTimer
{
    static constexpr std::chrono::milliseconds Tick{ 250 };

    std::mutex Mutex;
    std::condition_variable Wake;
    bool Stopping = false;
    std::thread Thread;

    FlushTimer()
    {
        Thread = std::thread([this]
        {
            std::unique_lock lock(Mutex);
            while (!Wake.wait_for(lock, Tick, [this] { return Stopping; }))
            {
                lock.unlock();
                LogFileRegistry::Instance().FlushDue(std::chrono::steady_clock::now());
                lock.lock();
            }
        });
    }

    ~FlushTimer()
    {
        {
            std::lock_guard lock(Mutex);
            Stopping = true;
        }
        Wake.notify_all();
        if (Thread.joinable())
            Thread.join();
    }
};

namespace
{
    struct DeferredMessage
    {
        std::shared_ptr<LogRouting const> Routing;
        uint16 LoggerIndex = 0;
        LogMessage Message;
        Appender const* Origin = nullptr;
    };

    enum class DeferredListState : uint8
    {
        Unused,
        Alive,
        Destroyed
    };

    thread_local uint32 DispatchDepth = 0;
    thread_local bool Draining = false;
    thread_local bool HasDeferred = false;
    thread_local Appender const* CurrentAppender = nullptr;
    thread_local DeferredListState ListState = DeferredListState::Unused;

    struct DeferredList
    {
        std::vector<DeferredMessage> Items;

        DeferredList()
        {
            ListState = DeferredListState::Alive;
        }

        ~DeferredList()
        {
            ListState = DeferredListState::Destroyed;
        }
    };

    std::vector<DeferredMessage>* GetDeferredList() noexcept
    {
        if (ListState == DeferredListState::Destroyed)
            return nullptr;
        thread_local DeferredList list;
        return &list.Items;
    }

    bool IsBuiltInType(AppenderType type) noexcept
    {
        return type == AppenderType::Console || type == AppenderType::File || type == AppenderType::Stream;
    }

    void FlushQuietly(Appender& appender) noexcept
    {
        try
        {
            appender.Flush();
        }
        catch (...)
        {
        }
    }

    LogLevel ToLevel(uint8 effective) noexcept
    {
        return effective > static_cast<uint8>(LogLevel::Fatal) ? LogLevel::Disabled : static_cast<LogLevel>(effective);
    }
}

Log::Log() : Log(ConsoleWriter::Instance())
{
}

Log::Log(ConsoleWriter& console) : _console(console), _streams(std::make_unique<LogStreamHub>())
{
    std::lock_guard lock(_applyMutex);
    RegisterBuiltInTypes();
    ApplyLocked(LogSettings::Bootstrap());
}

Log::~Log()
{
    DetachConfigWarnings();
    Shutdown();
}

Log& Log::Instance()
{
    static Log* const instance = []
    {
        Log* const log = new Log();
        std::atexit([] { Log::Instance().Shutdown(); });
        return log;
    }();
    return *instance;
}

void Log::RegisterBuiltInTypes()
{
    _registry.Register(AppenderConsole::GetTypeInfo());
    _registry.Register(AppenderFile::GetTypeInfo());
    _registry.Register(AppenderStream::GetTypeInfo());
}

LogConfigResult Log::LoadFromConfig(ConfigMgr const& config)
{
    LogConfigResult result;
    LogSettings settings = LogConfig::Read(config, result);
    if (!result.Succeeded())
        return result;
    std::lock_guard lock(_applyMutex);
    result.Merge(ApplyLocked(std::move(settings)));
    return result;
}

LogConfigResult Log::Apply(LogSettings settings)
{
    std::lock_guard lock(_applyMutex);
    return ApplyLocked(std::move(settings));
}

void Log::Reset()
{
    DetachConfigWarnings();
    std::lock_guard lock(_applyMutex);
    StopWorkerLocked();
    for (AppenderType const type : _registry.GetTypes())
        if (!IsBuiltInType(type))
            _registry.Unregister(type);
    _shutdown.store(false);
    ApplyLocked(LogSettings::Bootstrap());
}

void Log::AttachConfigWarnings(ConfigMgr& config)
{
    DetachConfigWarnings();
    {
        std::lock_guard lock(_applyMutex);
        _warningSource = &config;
    }
    config.SetWarningSink([this](std::string_view warning)
    {
        AMBROSE_LOG(*this, LogLevel::Warn, "server.config", "{}", warning);
    });
}

void Log::DetachConfigWarnings()
{
    ConfigMgr* source = nullptr;
    {
        std::lock_guard lock(_applyMutex);
        source = std::exchange(_warningSource, nullptr);
    }
    if (source)
        source->SetWarningSink(nullptr);
}

bool Log::Flush()
{
    if (DispatchDepth > 0)
        return false;
    while (true)
    {
        std::shared_ptr<State const> const state = LoadState();
        if (state->Worker && !state->Worker->IsWorkerThread())
        {
            auto const barrier = std::make_shared<LogBarrier>();
            LogWorkItem item{ state->Routing, LogRouting::RootIndex, LogMessage{}, barrier };
            LogWorker::PushResult const pushed = state->Worker->Push(item);
            if (pushed == LogWorker::PushResult::Queued)
            {
                if (barrier->WaitFor(FlushTimeout))
                    return true;
                _flushTimeouts.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            if (pushed == LogWorker::PushResult::Closed)
            {
                state->Worker->WaitUntilReleased();
                continue;
            }
        }
        FlushRouting(*state->Routing);
        _console.Flush();
        return true;
    }
}

void Log::Shutdown()
{
    std::lock_guard lock(_applyMutex);
    if (_shutdown.load())
        return;
    std::shared_ptr<State const> const state = LoadState();
    if (state->Worker && state->Worker->IsWorkerThread())
        return;
    StopWorkerLocked();
    _shutdown.store(true);
    _flushTimer.reset();
    FlushRouting(*LoadState()->Routing);
    _console.Flush();
    _console.Restore();
}

bool Log::IsShutdown() const noexcept
{
    return _shutdown.load();
}

LogConfigResult Log::RegisterAppenderType(AppenderTypeInfo info)
{
    std::lock_guard lock(_applyMutex);
    LogConfigResult result;
    AppenderType const type = info.Type;
    if (!_registry.Register(std::move(info)))
    {
        result.Errors.push_back(ConfigIssue{ {}, 0, fmt::format("appender type {} is already registered or has no factory", static_cast<uint32>(type)) });
        return result;
    }
    result = ApplyLocked(_settings);
    if (!result.Succeeded())
        _registry.Unregister(type);
    return result;
}

LogConfigResult Log::UnregisterAppenderType(AppenderType type)
{
    LogConfigResult result;
    std::weak_ptr<LogRouting const> previous;
    {
        std::lock_guard lock(_applyMutex);
        AppenderTypeInfo const* const registered = _registry.Find(type);
        if (IsBuiltInType(type) || registered == nullptr)
        {
            result.Errors.push_back(ConfigIssue{ {}, 0, fmt::format("appender type {} is not a registered module type", static_cast<uint32>(type)) });
            return result;
        }
        AppenderTypeInfo saved = *registered;
        _registry.Unregister(type);
        previous = LoadState()->Routing;
        result = ApplyLocked(_settings);
        if (!result.Succeeded())
        {
            _registry.Register(std::move(saved));
            return result;
        }
    }
    Flush();
    std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + FlushTimeout;
    while (!previous.expired())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            result.Errors.push_back(ConfigIssue{ {}, 0, fmt::format("timed out waiting for writes to appender type {} to finish", static_cast<uint32>(type)) });
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return result;
}

bool Log::IsAppenderTypeRegistered(AppenderType type) const
{
    std::lock_guard lock(_applyMutex);
    return _registry.Find(type) != nullptr;
}

bool Log::ShouldLog(std::string_view category, LogLevel level) const noexcept
{
    uint8 const value = static_cast<uint8>(level);
    if (value < _lowestLevel.load(std::memory_order_relaxed))
        return false;
    std::shared_ptr<State const> const state = LoadState();
    LogRouting const& routing = *state->Routing;
    return value >= routing.GetLogger(routing.Resolve(category)).GetEffectiveLevel();
}

LogLevel Log::GetEffectiveLevel(std::string_view category) const
{
    std::shared_ptr<State const> const state = LoadState();
    LogRouting const& routing = *state->Routing;
    return ToLevel(routing.GetLogger(routing.Resolve(category)).GetEffectiveLevel());
}

void Log::WriteText(std::string_view category, LogLevel level, std::string text) noexcept
{
    if (!ShouldLog(category, level))
        return;
    LogMessage message;
    message.Level = level;
    message.Category.assign(category.data(), category.size());
    message.Text = std::move(text);
    message.Time = std::chrono::system_clock::now();
    message.Sequence = _sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    message.ThreadId = LogMessage::CurrentOsThreadId();
    _written.fetch_add(1, std::memory_order_relaxed);
    Submit(nullptr, std::move(message));
    if (level == LogLevel::Fatal)
        Flush();
}

bool Log::SetLoggerLevel(std::string_view logger, LogLevel level)
{
    std::lock_guard lock(_applyMutex);
    LogSettings copy = _settings;
    auto const it = std::find_if(copy.Loggers.begin(), copy.Loggers.end(), [logger](LoggerDefinition const& definition) { return definition.Name == logger; });
    if (it == copy.Loggers.end())
        return false;
    it->Level = level;
    return ApplyLocked(std::move(copy)).Succeeded();
}

bool Log::SetAppenderLevel(std::string_view appender, LogLevel level)
{
    std::lock_guard lock(_applyMutex);
    LogSettings copy = _settings;
    auto const it = std::find_if(copy.Appenders.begin(), copy.Appenders.end(), [appender](AppenderDefinition const& definition) { return definition.Name == appender; });
    if (it == copy.Appenders.end())
        return false;
    it->Level = level;
    return ApplyLocked(std::move(copy)).Succeeded();
}

std::shared_ptr<Appender> Log::GetAppender(std::string_view name) const
{
    return LoadState()->Routing->FindAppender(name);
}

std::vector<std::string> Log::GetAppenderNames() const
{
    std::vector<std::string> names;
    for (std::shared_ptr<Appender> const& appender : LoadState()->Routing->GetAppenders())
        names.push_back(appender->GetName());
    return names;
}

std::vector<std::string> Log::GetLoggerNames() const
{
    std::vector<std::string> names;
    for (Logger const& logger : LoadState()->Routing->GetLoggers())
        names.push_back(logger.GetName());
    return names;
}

std::vector<std::string> Log::GetPendingAppenderNames() const
{
    std::vector<std::string> names;
    for (std::shared_ptr<Appender> const& appender : LoadState()->Routing->GetAppenders())
        if (auto const pending = std::dynamic_pointer_cast<AppenderPending>(appender); pending && !pending->HasSuccessor())
            names.push_back(appender->GetName());
    return names;
}

LogSettings Log::GetSettings() const
{
    std::lock_guard lock(_applyMutex);
    return _settings;
}

uint64 Log::GetGeneration() const noexcept
{
    return _generation.load(std::memory_order_relaxed);
}

LogStreamHub& Log::GetStreamHub() noexcept
{
    return *_streams;
}

LogErrorStore& Log::GetErrors() noexcept
{
    return _errors;
}

LogErrorStore const& Log::GetErrors() const noexcept
{
    return _errors;
}

ConsoleWriter& Log::GetConsole() noexcept
{
    return _console;
}

LogStatistics Log::GetStatistics() const
{
    std::shared_ptr<State const> const state = LoadState();
    LogStatistics statistics;
    statistics.Generation = state->Routing->GetGeneration();
    statistics.Async = state->Worker != nullptr;
    statistics.Written = _written.load();
    statistics.FormatErrors = _formatErrors.load();
    statistics.NestedDeferred = _nestedDeferred.load();
    statistics.NestedDropped = _nestedDropped.load();
    statistics.AsyncDropped = _asyncDropped.load();
    statistics.AsyncHighWater = state->Worker ? state->Worker->GetHighWaterMark() : 0;
    statistics.FlushTimeouts = _flushTimeouts.load();
    for (std::shared_ptr<Appender> const& appender : state->Routing->GetAppenders())
    {
        statistics.AppenderFailures += appender->GetFailureCount();
        if (auto const pending = std::dynamic_pointer_cast<AppenderPending>(appender))
            statistics.PendingDropped += pending->GetDroppedCount();
    }
    return statistics;
}

uint64 Log::RefreshSite(LogSite const& site) const noexcept
{
    std::shared_ptr<State const> const state = LoadState();
    LogRouting const& routing = *state->Routing;
    uint16 const index = routing.Resolve(site.GetCategory());
    uint64 const packed = LogSite::Pack(routing.GetGeneration(), index, routing.GetLogger(index).GetEffectiveLevel());
    site.StoreCache(packed);
    return packed;
}

void Log::WriteFormatted(LogSite const* site, std::string_view category, LogLevel level, fmt::string_view format, fmt::format_args args) noexcept
{
    LogMessage message;
    try
    {
        message.Text = fmt::vformat(format, args);
    }
    catch (std::exception const& exception)
    {
        _formatErrors.fetch_add(1, std::memory_order_relaxed);
        message.Text = fmt::format("<format error: {}> {}", exception.what(), std::string_view(format.data(), format.size()));
    }
    catch (...)
    {
        _formatErrors.fetch_add(1, std::memory_order_relaxed);
        message.Text = fmt::format("<format error> {}", std::string_view(format.data(), format.size()));
    }
    message.Level = level;
    message.Category.assign(category.data(), category.size());
    message.Time = std::chrono::system_clock::now();
    message.Sequence = _sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    message.ThreadId = LogMessage::CurrentOsThreadId();
    message.Template.assign(format.data(), format.size());
    if (site != nullptr)
        message.Source = site->GetSource();
    _written.fetch_add(1, std::memory_order_relaxed);
    Submit(site, std::move(message));
    if (level == LogLevel::Fatal)
        Flush();
}

void Log::Submit(LogSite const* site, LogMessage&& message) noexcept
{
    if (DispatchDepth > 0)
    {
        std::vector<DeferredMessage>* const list = GetDeferredList();
        if (Draining || list == nullptr || list->size() >= MaxDeferredPerDispatch)
        {
            _nestedDropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::shared_ptr<State const> const state = LoadState();
        message.Nested = true;
        uint16 const index = state->Routing->Resolve(message.Category);
        list->push_back(DeferredMessage{ state->Routing, index, std::move(message), CurrentAppender });
        HasDeferred = true;
        _nestedDeferred.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    while (true)
    {
        std::shared_ptr<State const> const state = LoadState();
        LogRouting const& routing = *state->Routing;
        uint16 index = LogRouting::RootIndex;
        uint64 const cached = site != nullptr ? site->LoadCache() : 0;
        if (site != nullptr && LogSite::GenerationOf(cached) == routing.GetGeneration())
            index = LogSite::LoggerIndexOf(cached);
        else
            index = routing.Resolve(message.Category);

        if (state->Worker && !state->Worker->IsWorkerThread())
        {
            LogWorkItem item{ state->Routing, index, std::move(message), nullptr };
            LogWorker::PushResult const pushed = state->Worker->Push(item);
            if (pushed == LogWorker::PushResult::Queued)
                return;
            if (pushed == LogWorker::PushResult::Dropped)
            {
                _asyncDropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            message = std::move(item.Message);
            state->Worker->WaitUntilReleased();
            continue;
        }

        Dispatch(state->Routing, index, message);
        if (_shutdown.load(std::memory_order_relaxed))
            for (uint16 const appender : routing.GetLogger(index).GetAppenders())
                FlushQuietly(*routing.GetAppenders()[appender]);
        return;
    }
}

void Log::Dispatch(std::shared_ptr<LogRouting const> const& routing, uint16 loggerIndex, LogMessage const& message) noexcept
{
    Logger const& logger = routing->GetLogger(loggerIndex);
    if (IsLevelEnabled(logger.GetLevel(), message.Level))
    {
        if (message.Level >= LogLevel::Error && message.Level != LogLevel::Disabled)
            _errors.Note(message);
        ++DispatchDepth;
        std::vector<std::shared_ptr<Appender>> const& appenders = routing->GetAppenders();
        for (uint16 const index : logger.GetAppenders())
        {
            Appender& appender = *appenders[index];
            if (!appender.Accepts(message))
                continue;
            Appender const* const previous = CurrentAppender;
            CurrentAppender = appender.GetSink();
            appender.Write(message);
            CurrentAppender = previous;
        }
        --DispatchDepth;
    }
    if (DispatchDepth == 0 && HasDeferred && !Draining)
        DrainDeferred();
}

void Log::DrainDeferred() noexcept
{
    HasDeferred = false;
    std::vector<DeferredMessage>* const list = GetDeferredList();
    if (list == nullptr || list->empty())
        return;
    std::vector<DeferredMessage> items;
    items.swap(*list);
    Draining = true;
    ++DispatchDepth;
    for (DeferredMessage const& deferred : items)
    {
        Logger const& logger = deferred.Routing->GetLogger(deferred.LoggerIndex);
        if (!IsLevelEnabled(logger.GetLevel(), deferred.Message.Level))
            continue;
        std::vector<std::shared_ptr<Appender>> const& appenders = deferred.Routing->GetAppenders();
        for (uint16 const index : logger.GetAppenders())
        {
            Appender& appender = *appenders[index];
            if (appender.GetSink() == deferred.Origin || !appender.Accepts(deferred.Message))
                continue;
            CurrentAppender = appender.GetSink();
            appender.Write(deferred.Message);
            CurrentAppender = nullptr;
        }
    }
    --DispatchDepth;
    Draining = false;
}

void Log::DispatchItem(LogWorkItem& item) noexcept
{
    if (item.Barrier)
    {
        FlushRouting(*item.Routing);
        _console.Flush();
        item.Barrier->Complete();
        return;
    }
    Dispatch(item.Routing, item.LoggerIndex, item.Message);
}

void Log::DispatchDropped(uint64 count, std::shared_ptr<LogRouting const> routing) noexcept
{
    try
    {
        if (!routing)
            routing = LoadState()->Routing;
        LogMessage message;
        message.Level = LogLevel::Warn;
        message.Category = "server.logging";
        message.Text = fmt::format("dropped {} log lines because the async queue was full", count);
        message.Time = std::chrono::system_clock::now();
        message.Sequence = _sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        message.ThreadId = LogMessage::CurrentOsThreadId();
        Dispatch(routing, routing->Resolve(message.Category), message);
    }
    catch (...)
    {
    }
}

void Log::FlushRouting(LogRouting const& routing) noexcept
{
    for (std::shared_ptr<Appender> const& appender : routing.GetAppenders())
        FlushQuietly(*appender);
}

std::shared_ptr<Log::State const> Log::LoadState() const
{
    std::lock_guard lock(_stateMutex);
    return _state;
}

void Log::Publish(std::shared_ptr<State const> next)
{
    uint8 const lowest = next->Routing->GetLowestLevel();
    uint64 const generation = next->Routing->GetGeneration();
    _lowestLevel.store(std::min(_lowestLevel.load(), lowest));
    std::shared_ptr<State const> previous;
    {
        std::lock_guard lock(_stateMutex);
        previous = std::exchange(_state, std::move(next));
    }
    _generation.store(generation);
    _lowestLevel.store(lowest);
}

std::shared_ptr<LogWorker> Log::MakeWorker(LogSettings const& settings)
{
    return std::make_shared<LogWorker>(settings.AsyncQueueSize, settings.AsyncQueueFull,
        [this](LogWorkItem& item) { DispatchItem(item); },
        [this](uint64 count, std::shared_ptr<LogRouting const> const& routing) { DispatchDropped(count, routing); },
        [] { LogFileRegistry::Instance().FlushDue(std::chrono::steady_clock::now()); });
}

void Log::StopWorkerLocked()
{
    std::shared_ptr<State const> const current = LoadState();
    if (!current || !current->Worker || current->Worker->IsWorkerThread())
        return;
    std::shared_ptr<LogWorker> const worker = current->Worker;
    worker->Close();
    worker->Join();
    auto next = std::make_shared<State>();
    next->Routing = current->Routing;
    Publish(std::move(next));
    worker->Release();
}

void Log::UpdateFlushTimerLocked(LogRouting const& routing, bool async)
{
    bool needed = false;
    if (!async && !_shutdown.load())
    {
        for (std::shared_ptr<Appender> const& appender : routing.GetAppenders())
        {
            AppenderFile const* const file = dynamic_cast<AppenderFile const*>(appender->GetSink());
            if (file != nullptr && file->GetFile()->GetFlushInterval().count() > 0)
            {
                needed = true;
                break;
            }
        }
    }
    if (needed && !_flushTimer)
        _flushTimer = std::make_unique<FlushTimer>();
    else if (!needed)
        _flushTimer.reset();
}

LogConfigResult Log::ApplyLocked(LogSettings settings)
{
    LogConfigResult result;
    std::shared_ptr<State const> const current = LoadState();

    bool rootSeen = false;
    std::map<std::string, uint16, std::less<>> definedAppenders;
    for (AppenderDefinition const& definition : settings.Appenders)
        if (!definedAppenders.emplace(definition.Name, uint16{ 0 }).second)
            result.Errors.push_back(definition.MakeIssue("is defined more than once"));
    for (LoggerDefinition const& definition : settings.Loggers)
    {
        rootSeen = rootSeen || definition.Name == "root";
        for (std::string const& name : definition.Appenders)
        {
            AppenderDefinition const* const appenderDefinition = settings.FindAppender(name);
            if (!appenderDefinition)
            {
                result.Errors.push_back(definition.MakeIssue(fmt::format("references appender '{}', which is not defined", name)));
                continue;
            }
            if (AppenderTypeInfo const* const info = _registry.Find(appenderDefinition->Type))
                for (std::string const& excluded : info->ExcludedCategories)
                    if (Ambrose::Logging::IsCategoryWithin(definition.Name, excluded))
                        result.Errors.push_back(definition.MakeIssue(fmt::format("routes to appender '{}', whose type never accepts category '{}'", name, excluded)));
        }
    }
    if (!rootSeen)
        result.Errors.push_back(ConfigIssue{ {}, 0, "Logger.root is required, for example Logger.root = 3,Console Server" });

    std::shared_ptr<LogWorker> const worker = current ? current->Worker : nullptr;
    bool const wantAsync = settings.AsyncEnable && !_shutdown.load();
    bool const keepWorker = worker && wantAsync && worker->GetCapacity() == settings.AsyncQueueSize && worker->GetPolicy() == settings.AsyncQueueFull;
    bool const changeWorker = !keepWorker && (worker || wantAsync);
    if (changeWorker && worker && worker->IsWorkerThread())
        result.Errors.push_back(ConfigIssue{ {}, 0, "async logging settings cannot change from inside an appender on the logging thread" });
    if (!result.Succeeded())
        return result;

    std::stable_partition(settings.Loggers.begin(), settings.Loggers.end(), [](LoggerDefinition const& logger) { return logger.Name == "root"; });

    bool const needsLogsDir = std::any_of(settings.Appenders.begin(), settings.Appenders.end(), [this](AppenderDefinition const& definition)
    {
        return definition.Type == AppenderType::File && _registry.Find(AppenderType::File) != nullptr;
    });
    if (needsLogsDir && !settings.LogsDir.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(settings.LogsDir, error);
        if (error)
        {
            result.Errors.push_back(ConfigIssue{ {}, 0, fmt::format("LogsDir: cannot create {}: {}", ConfigMgr::PathToUtf8(settings.LogsDir), error.message()) });
            return result;
        }
    }

    LogLayout layout;
    layout.Timestamp = settings.ConsoleTimestamp;
    layout.CategoryWidth = settings.ConsoleCategoryWidth;
    AppenderCreateContext const context{ settings.LogsDir, std::chrono::system_clock::now(), settings.Utc, _console, LogFileRegistry::Instance(), *_streams, layout, settings.ConsoleRepeatCategory };
    std::string const sharedKey = fmt::format("\x1F{}\x1F{}", settings.Utc ? 1 : 0, ConfigMgr::PathToUtf8(settings.LogsDir));
    std::vector<std::shared_ptr<Appender>> appenders;
    std::vector<std::shared_ptr<Appender>> created;
    std::vector<std::pair<std::shared_ptr<Appender>, LogLevel>> levelUpdates;
    std::vector<std::pair<std::shared_ptr<AppenderPending>, std::shared_ptr<Appender>>> handovers;
    std::map<std::string, std::string, std::less<>> reuseKeys;
    std::map<std::string, uint16, std::less<>> appenderIndex;

    for (AppenderDefinition const& definition : settings.Appenders)
    {
        AppenderTypeInfo const* const info = _registry.Find(definition.Type);
        std::string key = definition.ReuseKey() + sharedKey;
        if (!info)
            key += fmt::format("\x1F{}", settings.PendingBuffer);
        std::shared_ptr<Appender> const existing = current ? current->Routing->FindAppender(definition.Name) : nullptr;
        auto const previousKey = _appliedReuseKeys.find(definition.Name);
        bool const sameKey = existing && existing->GetType() == definition.Type && previousKey != _appliedReuseKeys.end() && previousKey->second == key;
        std::shared_ptr<AppenderPending> const pending = std::dynamic_pointer_cast<AppenderPending>(existing);

        std::shared_ptr<Appender> appender;
        if (info)
        {
            if (sameKey && !pending)
                appender = existing;
            else
            {
                try
                {
                    appender = info->Create(definition, context, result);
                }
                catch (std::exception const& exception)
                {
                    result.Errors.push_back(definition.MakeIssue(fmt::format("could not be created: {}", exception.what())));
                }
                if (!appender)
                    continue;
                appender->SetExcludedCategories(info->ExcludedCategories);
                created.push_back(appender);
                if (pending && pending->GetType() == definition.Type && !pending->HasSuccessor())
                    handovers.emplace_back(pending, appender);
            }
        }
        else
        {
            if (sameKey && pending && !pending->HasSuccessor())
                appender = existing;
            else
                appender = std::make_shared<AppenderPending>(definition, settings.PendingBuffer);
            result.InactiveAppenders.push_back(definition.Name);
        }
        levelUpdates.emplace_back(appender, definition.Level);
        appenderIndex.emplace(definition.Name, static_cast<uint16>(appenders.size()));
        appenders.push_back(std::move(appender));
        reuseKeys[definition.Name] = std::move(key);
    }
    if (!result.Succeeded())
        return result;

    std::vector<Logger> loggers;
    loggers.reserve(settings.Loggers.size());
    for (LoggerDefinition const& definition : settings.Loggers)
    {
        std::vector<uint16> indices;
        uint8 lowestAppender = LogLevelNever;
        for (std::string const& name : definition.Appenders)
        {
            auto const it = appenderIndex.find(name);
            if (it == appenderIndex.end())
                continue;
            if (std::find(indices.begin(), indices.end(), it->second) == indices.end())
                indices.push_back(it->second);
            LogLevel const level = settings.FindAppender(name)->Level;
            if (level != LogLevel::Disabled)
                lowestAppender = std::min(lowestAppender, static_cast<uint8>(level));
        }
        uint8 effective = LogLevelNever;
        if (definition.Level != LogLevel::Disabled && lowestAppender != LogLevelNever)
            effective = std::max(static_cast<uint8>(definition.Level), lowestAppender);
        loggers.emplace_back(definition.Name, definition.Level, std::move(indices), effective);
    }

    for (std::shared_ptr<Appender> const& appender : created)
    {
        try
        {
            appender->Activate();
        }
        catch (...)
        {
        }
    }
    for (auto const& [appender, level] : levelUpdates)
        appender->SetLevel(level);
    for (auto const& [pending, successor] : handovers)
    {
        ++DispatchDepth;
        Appender const* const previous = CurrentAppender;
        CurrentAppender = successor.get();
        pending->HandOver(successor);
        CurrentAppender = previous;
        --DispatchDepth;
    }
    if (DispatchDepth == 0 && HasDeferred && !Draining)
        DrainDeferred();
    _console.SetColorMode(settings.ConsoleColors);

    auto const routing = std::make_shared<LogRouting const>(LogRouting::NextGeneration(), std::move(loggers), std::move(appenders), settings.Utc);
    auto next = std::make_shared<State>();
    next->Routing = routing;
    bool async = worker != nullptr;
    if (!changeWorker)
    {
        next->Worker = worker;
        Publish(std::move(next));
    }
    else
    {
        next->Worker = wantAsync ? MakeWorker(settings) : nullptr;
        async = next->Worker != nullptr;
        if (worker)
        {
            worker->Close();
            worker->Join();
        }
        Publish(std::move(next));
        if (worker)
            worker->Release();
    }
    UpdateFlushTimerLocked(*routing, async);
    _settings = std::move(settings);
    _appliedReuseKeys = std::move(reuseKeys);
    return result;
}
