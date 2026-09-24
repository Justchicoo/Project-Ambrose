/*
 * Project Ambrose by Imjustchico
 * Resolves categories to loggers by walking dot prefixes, and hands out process-unique routing generations.
 */

#include "LogRouting.h"
#include "Appender.h"

#include <algorithm>
#include <atomic>

LogRouting::LogRouting(uint64 generation, std::vector<Logger> loggers, std::vector<std::shared_ptr<Appender>> appenders, bool utc)
    : _generation(generation), _lowestLevel(LogLevelNever), _utc(utc), _loggers(std::move(loggers)), _appenders(std::move(appenders))
{
    for (std::size_t i = 0; i < _loggers.size(); ++i)
    {
        _index.emplace(_loggers[i].GetName(), static_cast<uint16>(i));
        _lowestLevel = std::min(_lowestLevel, _loggers[i].GetEffectiveLevel());
    }
}

uint64 LogRouting::GetGeneration() const noexcept
{
    return _generation;
}

uint8 LogRouting::GetLowestLevel() const noexcept
{
    return _lowestLevel;
}

bool LogRouting::IsUtc() const noexcept
{
    return _utc;
}

uint16 LogRouting::Resolve(std::string_view category) const noexcept
{
    while (!category.empty())
    {
        auto const it = _index.find(category);
        if (it != _index.end())
            return it->second;
        std::size_t const dot = category.rfind('.');
        if (dot == std::string_view::npos)
            break;
        category = category.substr(0, dot);
    }
    return RootIndex;
}

Logger const& LogRouting::GetLogger(uint16 index) const noexcept
{
    return index < _loggers.size() ? _loggers[index] : _loggers[RootIndex];
}

std::vector<Logger> const& LogRouting::GetLoggers() const noexcept
{
    return _loggers;
}

std::vector<std::shared_ptr<Appender>> const& LogRouting::GetAppenders() const noexcept
{
    return _appenders;
}

std::shared_ptr<Appender> LogRouting::FindAppender(std::string_view name) const
{
    for (std::shared_ptr<Appender> const& appender : _appenders)
        if (appender->GetName() == name)
            return appender;
    return nullptr;
}

uint64 LogRouting::NextGeneration() noexcept
{
    static std::atomic<uint64> next{ 1 };
    return next.fetch_add(1, std::memory_order_relaxed);
}
