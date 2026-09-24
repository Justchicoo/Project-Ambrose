/*
 * Project Ambrose by Imjustchico
 * The sLog singleton, the LOG_* macros, type registration, routing, deferred nested delivery, reload, flush and shutdown.
 */

#ifndef AMBROSE_LOG_H
#define AMBROSE_LOG_H

#include "AppenderRegistry.h"
#include "LogCommon.h"
#include "LogErrors.h"
#include "LogConfig.h"
#include "LogSite.h"

#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#if !FMT_USE_CONSTEVAL
#error "Project Ambrose logging requires fmt compile-time format checks: use MSVC 19.40 or newer, GCC 10.2 or newer, or Clang 11.1 or newer"
#endif

class ConsoleWriter;
class LogRouting;
class LogStreamHub;
class LogWorker;
struct LogWorkItem;

struct LogStatistics
{
    uint64 Generation = 0;
    bool Async = false;
    uint64 Written = 0;
    uint64 FormatErrors = 0;
    uint64 NestedDeferred = 0;
    uint64 NestedDropped = 0;
    uint64 AsyncDropped = 0;
    uint64 AsyncHighWater = 0;
    uint64 FlushTimeouts = 0;
    uint64 PendingDropped = 0;
    uint64 AppenderFailures = 0;
};

class Log
{
public:
    static constexpr std::chrono::milliseconds FlushTimeout{ 10000 };
    static constexpr std::size_t MaxDeferredPerDispatch = 64;

    Log();
    explicit Log(ConsoleWriter& console);
    ~Log();

    Log(Log const&) = delete;
    Log& operator=(Log const&) = delete;

    static Log& Instance();

    LogConfigResult LoadFromConfig(ConfigMgr const& config);
    LogConfigResult Apply(LogSettings settings);
    void Reset();
    void AttachConfigWarnings(ConfigMgr& config);
    void DetachConfigWarnings();
    bool Flush();
    void Shutdown();
    bool IsShutdown() const noexcept;

    template<AppenderImplementation T>
    LogConfigResult RegisterAppender()
    {
        return RegisterAppenderType(T::GetTypeInfo());
    }

    LogConfigResult RegisterAppenderType(AppenderTypeInfo info);
    LogConfigResult UnregisterAppenderType(AppenderType type);
    bool IsAppenderTypeRegistered(AppenderType type) const;

    bool ShouldLog(LogSite const& site, LogLevel level) const noexcept
    {
        uint8 const value = static_cast<uint8>(level);
        if (value < _lowestLevel.load(std::memory_order_relaxed))
            return false;
        uint64 cached = site.LoadCache();
        if (LogSite::GenerationOf(cached) != _generation.load(std::memory_order_relaxed))
            cached = RefreshSite(site);
        return value >= LogSite::LevelOf(cached);
    }

    bool ShouldLog(std::string_view category, LogLevel level) const noexcept;
    LogLevel GetEffectiveLevel(std::string_view category) const;

    template<typename... Args>
    void Write(LogSite const& site, LogLevel level, fmt::format_string<Args...> format, Args&&... args) noexcept
    {
        WriteFormatted(&site, site.GetCategory(), level, format.get(), fmt::make_format_args(args...));
    }

    template<typename... Args>
    void Write(std::string_view category, LogLevel level, fmt::format_string<Args...> format, Args&&... args) noexcept
    {
        WriteFormatted(nullptr, category, level, format.get(), fmt::make_format_args(args...));
    }

    void WriteText(std::string_view category, LogLevel level, std::string text) noexcept;

    bool SetLoggerLevel(std::string_view logger, LogLevel level);
    bool SetAppenderLevel(std::string_view appender, LogLevel level);
    std::shared_ptr<Appender> GetAppender(std::string_view name) const;
    std::vector<std::string> GetAppenderNames() const;
    std::vector<std::string> GetLoggerNames() const;
    std::vector<std::string> GetPendingAppenderNames() const;
    LogSettings GetSettings() const;
    uint64 GetGeneration() const noexcept;
    LogStreamHub& GetStreamHub() noexcept;
    LogErrorStore& GetErrors() noexcept;
    LogErrorStore const& GetErrors() const noexcept;
    ConsoleWriter& GetConsole() noexcept;
    LogStatistics GetStatistics() const;

private:
    struct State;
    struct FlushTimer;

    uint64 RefreshSite(LogSite const& site) const noexcept;
    void WriteFormatted(LogSite const* site, std::string_view category, LogLevel level, fmt::string_view format, fmt::format_args args) noexcept;
    void Submit(LogSite const* site, LogMessage&& message) noexcept;
    void Dispatch(std::shared_ptr<LogRouting const> const& routing, uint16 loggerIndex, LogMessage const& message) noexcept;
    void DispatchItem(LogWorkItem& item) noexcept;
    void DispatchDropped(uint64 count, std::shared_ptr<LogRouting const> routing) noexcept;
    void DrainDeferred() noexcept;
    void FlushRouting(LogRouting const& routing) noexcept;
    std::shared_ptr<State const> LoadState() const;
    void Publish(std::shared_ptr<State const> next);
    LogConfigResult ApplyLocked(LogSettings settings);
    std::shared_ptr<LogWorker> MakeWorker(LogSettings const& settings);
    void StopWorkerLocked();
    void UpdateFlushTimerLocked(LogRouting const& routing, bool async);
    void RegisterBuiltInTypes();

    ConsoleWriter& _console;
    std::unique_ptr<LogStreamHub> _streams;
    LogErrorStore _errors;
    mutable std::mutex _stateMutex;
    std::shared_ptr<State const> _state;
    std::atomic<uint64> _generation{ 0 };
    std::atomic<uint8> _lowestLevel{ static_cast<uint8>(LogLevel::Info) };
    mutable std::mutex _applyMutex;
    AppenderRegistry _registry;
    LogSettings _settings;
    std::map<std::string, std::string, std::less<>> _appliedReuseKeys;
    std::unique_ptr<FlushTimer> _flushTimer;
    std::atomic<uint64> _sequence{ 0 };
    std::atomic<bool> _shutdown{ false };
    ConfigMgr* _warningSource = nullptr;
    mutable std::atomic<uint64> _written{ 0 };
    mutable std::atomic<uint64> _formatErrors{ 0 };
    mutable std::atomic<uint64> _nestedDeferred{ 0 };
    mutable std::atomic<uint64> _nestedDropped{ 0 };
    mutable std::atomic<uint64> _asyncDropped{ 0 };
    mutable std::atomic<uint64> _flushTimeouts{ 0 };
};

#define sLog Log::Instance()

#define AMBROSE_LOG(log, level, filter, ...) \
    do \
    { \
        static constinit LogSite const ambroseLogSite_((filter), AMBROSE_LOG_SOURCE); \
        if ((log).ShouldLog(ambroseLogSite_, (level))) \
            (log).Write(ambroseLogSite_, (level), __VA_ARGS__); \
    } while (false)

#define LOG_TRACE(filter, ...) AMBROSE_LOG(sLog, LogLevel::Trace, filter, __VA_ARGS__)
#define LOG_DEBUG(filter, ...) AMBROSE_LOG(sLog, LogLevel::Debug, filter, __VA_ARGS__)
#define LOG_INFO(filter, ...) AMBROSE_LOG(sLog, LogLevel::Info, filter, __VA_ARGS__)
#define LOG_WARN(filter, ...) AMBROSE_LOG(sLog, LogLevel::Warn, filter, __VA_ARGS__)
#define LOG_ERROR(filter, ...) AMBROSE_LOG(sLog, LogLevel::Error, filter, __VA_ARGS__)
#define LOG_FATAL(filter, ...) AMBROSE_LOG(sLog, LogLevel::Fatal, filter, __VA_ARGS__)

#define LOG_DYNAMIC(level, filter, ...) \
    do \
    { \
        auto&& ambroseLogCategory_ = (filter); \
        if (sLog.ShouldLog(std::string_view(ambroseLogCategory_), (level))) \
            sLog.Write(std::string_view(ambroseLogCategory_), (level), __VA_ARGS__); \
    } while (false)

#endif
