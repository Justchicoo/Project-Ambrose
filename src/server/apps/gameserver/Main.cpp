/*
 * Project Ambrose by Imjustchico
 * Game server entry point: loads gameserver.conf, starts logging, logs the banner and any config issues, and exits.
 */

#include "Banner.h"
#include "ConfigMgr.h"
#include "Log.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string> const arguments(argv, argv + argc);
    ConfigLoadResult const config = sConfigMgr.LoadInitial("gameserver.conf", arguments);
    LogConfigResult logResult;
    if (config.Succeeded())
        logResult = sLog.LoadFromConfig(sConfigMgr);

    Ambrose::Banner::Show("gameserver", [](std::string_view line) { LOG_INFO("server.gameserver", "{}", line); });
    for (ConfigIssue const& issue : config.Errors)
        LOG_ERROR("server.config", "{}", issue.ToString());
    for (ConfigIssue const& issue : logResult.Warnings)
        LOG_WARN("server.logging", "{}", issue.ToString());
    for (ConfigIssue const& issue : logResult.Errors)
        LOG_ERROR("server.logging", "{}", issue.ToString());
    sLog.AttachConfigWarnings(sConfigMgr);
    for (std::string const& name : sLog.GetPendingAppenderNames())
        LOG_WARN("server.logging", "appender '{}' uses a type no layer in this app registers; it stays inactive", name);

    bool const succeeded = config.Succeeded() && logResult.Succeeded();
    if (succeeded)
        LOG_INFO("server.gameserver", "Logging to {}", ConfigMgr::PathToUtf8(sLog.GetSettings().LogsDir));
    sLog.DetachConfigWarnings();
    sLog.Shutdown();
    return succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
