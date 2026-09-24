/*
 * Project Ambrose by Imjustchico
 * Reads the record from the end, because the newest rows are the ones an operator came for and a record that has run for weeks should not have to be read whole to show the last twenty. A line that is not a JSON object is counted as unreadable and passed over, so one bad write cannot hide everything written after it, and the count is reported rather than swallowed.
 */

#include "AdminActivityView.h"
#include "AdminRouter.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    std::vector<std::string> ReadLines(std::filesystem::path const& file)
    {
        std::vector<std::string> lines;
        std::ifstream in(file, std::ios::binary);
        if (!in)
            return lines;
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!Ambrose::Trim(line).empty())
                lines.push_back(line);
        }
        return lines;
    }
}

std::string AdminActivityView::ActivityJson(std::filesystem::path const& file, std::size_t limit)
{
    limit = std::clamp<std::size_t>(limit, 1, MaxLimit);

    nlohmann::json body;
    body["schema"] = SchemaVersion;

    std::error_code code;
    bool const there = !file.empty() && std::filesystem::exists(file, code);
    body["kept"] = there;

    std::vector<std::string> const lines = there ? ReadLines(file) : std::vector<std::string>{};
    body["written"] = lines.size();

    nlohmann::json rows = nlohmann::json::array();
    std::size_t unreadable = 0;
    for (auto line = lines.rbegin(); line != lines.rend() && rows.size() < limit; ++line)
    {
        nlohmann::json row = nlohmann::json::parse(*line, nullptr, false);
        if (!row.is_object())
        {
            ++unreadable;
            continue;
        }
        rows.push_back(std::move(row));
    }
    body["unreadable"] = unreadable;
    body["activity"] = std::move(rows);
    return body.dump();
}

void AdminActivityView::Register(AdminRouter& router, std::filesystem::path auditFile)
{
    router.AddGuarded("GET", "/api/activity", "audit.read", [file = std::move(auditFile)](AdminRequest const& request)
    {
        std::string_view const asked = request.Query("limit");
        if (asked.empty())
            return AdminResponse::Json(200, ActivityJson(file, DefaultLimit));
        std::optional<std::size_t> const wanted = Ambrose::StringTo<std::size_t>(asked);
        if (!wanted || *wanted == 0 || *wanted > MaxLimit)
            return AdminResponse::Invalid("Reading the record takes a whole number of rows",
                { { "limit", fmt::format("Give a whole number from 1 to {}", MaxLimit) } });
        return AdminResponse::Json(200, ActivityJson(file, *wanted));
    });
}
