/*
 * Project Ambrose by Imjustchico
 * Parses inline logging config text through ConfigMgr so tests exercise the real grammar and layers.
 */

#include "LogTestConfig.h"

#include <gtest/gtest.h>

namespace
{
    std::string const Header = "# Project Ambrose by Imjustchico\n# Logging test configuration.\n";
}

std::vector<std::pair<std::string, ConfigEntry>> LogTestConfig::Entries(std::string_view body)
{
    ParsedConfig parsed = ConfigMgr::ParseText(Header + std::string(body), "app.conf", ConfigSourceKind::Config);
    EXPECT_TRUE(parsed.Errors.empty()) << (parsed.Errors.empty() ? std::string() : parsed.Errors.front().ToString());
    return std::move(parsed.Entries);
}

LogSettings LogTestConfig::Settings(std::string_view body, LogConfigResult& result)
{
    return LogConfig::Parse(Entries(body), result);
}

LogSettings LogTestConfig::Settings(std::string_view body)
{
    LogConfigResult result;
    LogSettings settings = Settings(body, result);
    EXPECT_TRUE(result.Succeeded()) << Describe(result);
    return settings;
}

std::unique_ptr<ConfigMgr> LogTestConfig::Load(LogTestDirectory const& directory, std::string_view body, ConfigMgr::EnvironmentLookup environment)
{
    if (!environment)
        environment = [](std::string const&) -> std::optional<std::string> { return std::nullopt; };
    std::filesystem::path const file = directory.Write("app.conf", Header + std::string(body));
    auto config = std::make_unique<ConfigMgr>(std::move(environment));
    ConfigLoadResult const loaded = config->LoadInitial(file);
    EXPECT_TRUE(loaded.Succeeded()) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front().ToString());
    return config;
}

std::string LogTestConfig::Describe(LogConfigResult const& result)
{
    std::string text;
    for (ConfigIssue const& issue : result.Errors)
        text += "error: " + issue.ToString() + "\n";
    for (ConfigIssue const& issue : result.Warnings)
        text += "warning: " + issue.ToString() + "\n";
    return text;
}
