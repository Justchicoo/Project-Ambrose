/*
 * Project Ambrose by Imjustchico
 * A configured category: its name, level, effective level and the appenders it routes to.
 */

#ifndef AMBROSE_LOGGER_H
#define AMBROSE_LOGGER_H

#include "LogCommon.h"

#include <span>
#include <string>
#include <vector>

class Logger
{
public:
    Logger(std::string name, LogLevel level, std::vector<uint16> appenders, uint8 effectiveLevel);

    std::string const& GetName() const noexcept;
    LogLevel GetLevel() const noexcept;
    uint8 GetEffectiveLevel() const noexcept;
    std::span<uint16 const> GetAppenders() const noexcept;

private:
    std::string _name;
    LogLevel _level;
    uint8 _effectiveLevel;
    std::vector<uint16> _appenders;
};

#endif
