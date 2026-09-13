/*
 * Project Ambrose by Imjustchico
 * Immutable per-generation map from categories to their nearest configured logger and its appenders.
 */

#ifndef AMBROSE_LOGROUTING_H
#define AMBROSE_LOGROUTING_H

#include "Logger.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Appender;

class LogRouting
{
public:
    static constexpr uint16 RootIndex = 0;

    LogRouting(uint64 generation, std::vector<Logger> loggers, std::vector<std::shared_ptr<Appender>> appenders, bool utc);

    LogRouting(LogRouting const&) = delete;
    LogRouting& operator=(LogRouting const&) = delete;

    uint64 GetGeneration() const noexcept;
    uint8 GetLowestLevel() const noexcept;
    bool IsUtc() const noexcept;
    uint16 Resolve(std::string_view category) const noexcept;
    Logger const& GetLogger(uint16 index) const noexcept;
    std::vector<Logger> const& GetLoggers() const noexcept;
    std::vector<std::shared_ptr<Appender>> const& GetAppenders() const noexcept;
    std::shared_ptr<Appender> FindAppender(std::string_view name) const;

    static uint64 NextGeneration() noexcept;

private:
    uint64 _generation;
    uint8 _lowestLevel;
    bool _utc;
    std::vector<Logger> _loggers;
    std::map<std::string, uint16, std::less<>> _index;
    std::vector<std::shared_ptr<Appender>> _appenders;
};

#endif
