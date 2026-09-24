/*
 * Project Ambrose by Imjustchico
 * Formats the startup banner lines with the app name and full revision.
 */

#include "Banner.h"
#include "GitRevision.h"

#include <fmt/format.h>

void Ambrose::Banner::Show(std::string_view appName, std::function<void(std::string_view)> const& log)
{
    log("Project Ambrose by Imjustchico");
    log(fmt::format("An AI-built Wizard101 server: {}", appName));
    log(GitRevision::GetFullVersion());
}
