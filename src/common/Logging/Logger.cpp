/*
 * Project Ambrose by Imjustchico
 * Stores one configured logger's name, level, effective level, and appender indices.
 */

#include "Logger.h"

#include <utility>

Logger::Logger(std::string name, LogLevel level, std::vector<uint16> appenders, uint8 effectiveLevel)
    : _name(std::move(name)), _level(level), _effectiveLevel(effectiveLevel), _appenders(std::move(appenders))
{
}

std::string const& Logger::GetName() const noexcept
{
    return _name;
}

LogLevel Logger::GetLevel() const noexcept
{
    return _level;
}

uint8 Logger::GetEffectiveLevel() const noexcept
{
    return _effectiveLevel;
}

std::span<uint16 const> Logger::GetAppenders() const noexcept
{
    return _appenders;
}
