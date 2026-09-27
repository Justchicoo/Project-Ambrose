/*
 * Project Ambrose by Imjustchico
 * Builds the versioned error report with build and operating-system identity, the selected apps' revisions and source locations, omitting all rendered values unless the preview explicitly includes them.
 */

#include "PanelErrorReport.h"

#include "GitRevision.h"
#include "LogRedaction.h"

#include <nlohmann/json.hpp>

#include <cstddef>

namespace
{
    constexpr std::size_t MaxReportBytes = 4 * 1024 * 1024;

    char const* OperatingSystem()
    {
#ifdef _WIN32
        return "Windows";
#elif defined(__APPLE__)
        return "macOS";
#elif defined(__linux__)
        return "Linux";
#else
        return "unknown";
#endif
    }
}

std::optional<nlohmann::json> PanelErrorReport::Build(std::vector<PanelErrorGroup> const& groups, bool includeRendered, std::string& error)
{
    error.clear();
    if (groups.empty())
    {
        error = "the report needs at least one error group";
        return std::nullopt;
    }
    if (groups.size() > 200)
    {
        error = "the report cannot contain more than 200 error groups";
        return std::nullopt;
    }

    nlohmann::json report;
    report["format"] = "project-ambrose-error-report";
    report["schema"] = 1;
    report["product"] = {
        { "name", "Project Ambrose" },
        { "version", GitRevision::GetFullVersion() },
        { "commit", GitRevision::GetHash() },
        { "branch", GitRevision::GetBranch() }
    };
    report["operating_system"] = OperatingSystem();
    report["apps"] = nlohmann::json::object();
    report["groups"] = nlohmann::json::array();

    for (PanelErrorGroup const& group : groups)
    {
        report["apps"][group.App] = group.Revision;
        nlohmann::json entry;
        entry["app"] = group.App;
        entry["revision"] = group.Revision;
        entry["level"] = group.Level;
        entry["category"] = group.Category;
        entry["source"] = { { "file", group.File }, { "line", group.Line }, { "function", group.Function } };
        entry["template"] = group.Template;
        entry["count"] = group.Count;
        entry["total_count"] = group.TotalCount;
        entry["first_epoch_ms"] = group.FirstEpochMs;
        entry["last_epoch_ms"] = group.LastEpochMs;
        if (includeRendered)
        {
            nlohmann::json context = nlohmann::json::parse(group.ContextBeforeJson, nullptr, false);
            if (!context.is_array())
            {
                error = "the saved log context for an error group is not a JSON array";
                return std::nullopt;
            }
            for (nlohmann::json& line : context)
            {
                if (!line.is_object() || !line.contains("message") || !line["message"].is_string())
                {
                    error = "the saved log context for an error group has an invalid record";
                    return std::nullopt;
                }
                line["message"] = LogRedaction::Redact(line["message"].get<std::string>());
            }
            entry["rendered_message"] = LogRedaction::Redact(group.LastMessage);
            entry["log_lines_before"] = std::move(context);
        }
        report["groups"].push_back(std::move(entry));
    }
    if (report.dump().size() > MaxReportBytes)
    {
        error = "the report is larger than the 4 MiB limit";
        return std::nullopt;
    }
    return report;
}
