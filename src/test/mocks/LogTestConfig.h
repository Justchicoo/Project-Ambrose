/*
 * Project Ambrose by Imjustchico
 * Builds config entries, LogSettings and loaded ConfigMgr instances from inline config text for logging tests.
 */

#ifndef AMBROSE_LOGTESTCONFIG_H
#define AMBROSE_LOGTESTCONFIG_H

#include "LogConfig.h"
#include "LogTestDirectory.h"

#include <memory>
#include <string_view>

class LogTestConfig
{
public:
    static std::vector<std::pair<std::string, ConfigEntry>> Entries(std::string_view body);
    static LogSettings Settings(std::string_view body, LogConfigResult& result);
    static LogSettings Settings(std::string_view body);
    static std::unique_ptr<ConfigMgr> Load(LogTestDirectory const& directory, std::string_view body, ConfigMgr::EnvironmentLookup environment = {});
    static std::string Describe(LogConfigResult const& result);
};

#endif
