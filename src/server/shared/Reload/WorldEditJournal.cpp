/*
 * Project Ambrose by Imjustchico
 * Keeps the edits and writes them out: the newest are kept when the journal is full, because what a server was told to do a moment ago matters more than what it was told at the start of a long run, and the export is named the way every other update file is named, counting within the day so two exports on one day do not collide. The file carries the statements in the order they were made, each under a line saying who made it and when, so a reviewer reads what happened rather than a list of SQL with no account of itself.
 */

#include "WorldEditJournal.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

namespace
{
    int64 NowMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

WorldEditJournal& WorldEditJournal::Instance()
{
    static WorldEditJournal instance;
    return instance;
}

void WorldEditJournal::Record(std::string who, std::string source, std::string statement)
{
    WorldEdit edit;
    edit.EpochMs = NowMilliseconds();
    edit.Who = std::move(who);
    edit.Source = std::move(source);
    edit.Statement = std::move(statement);
    Record(std::move(edit));
}

void WorldEditJournal::Record(WorldEdit edit)
{
    if (Ambrose::Trim(edit.Statement).empty())
        return;
    if (edit.EpochMs == 0)
        edit.EpochMs = NowMilliseconds();
    if (edit.Who.empty())
        edit.Who = "somebody unnamed";
    if (edit.Source.empty())
        edit.Source = "somewhere unnamed";
    std::lock_guard const lock(_mutex);
    _edits.push_back(std::move(edit));
    if (_edits.size() > MaxEntries)
        _edits.erase(_edits.begin(), _edits.begin() + static_cast<std::ptrdiff_t>(_edits.size() - MaxEntries));
}

std::vector<WorldEdit> WorldEditJournal::Entries() const
{
    std::lock_guard const lock(_mutex);
    return _edits;
}

std::size_t WorldEditJournal::Count() const
{
    std::lock_guard const lock(_mutex);
    return _edits.size();
}

void WorldEditJournal::Clear()
{
    std::lock_guard const lock(_mutex);
    _edits.clear();
}

std::string WorldEditJournal::StampOf(int64 epochMs)
{
    std::tm parts{};
    if (!LogTimestamp::BreakDown(static_cast<std::time_t>(epochMs / 1000), true, parts))
        return "an unreadable time";
    return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}Z", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
        parts.tm_hour, parts.tm_min, parts.tm_sec);
}

std::string WorldEditJournal::Render(std::vector<WorldEdit> const& edits)
{
    std::string text = "-- Project Ambrose by Imjustchico\n";
    text += fmt::format("-- {} world edit(s) made while a server was running, exported from its journal.\n", edits.size());
    for (WorldEdit const& edit : edits)
    {
        text += fmt::format("-- {} by {} from {}\n", StampOf(edit.EpochMs), edit.Who, edit.Source);
        std::string statement = std::string(Ambrose::Trim(edit.Statement));
        if (!statement.empty() && statement.back() != ';')
            statement += ';';
        text += statement;
        text += '\n';
    }
    return text;
}

std::string WorldEditJournal::NextFileName(std::filesystem::path const& folder, int64 epochMs)
{
    std::tm parts{};
    if (!LogTimestamp::BreakDown(static_cast<std::time_t>(epochMs / 1000), true, parts))
        parts = std::tm{};
    std::string const day = fmt::format("{:04}_{:02}_{:02}", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday);
    std::error_code code;
    for (int counter = 0; counter < 100; ++counter)
    {
        std::string const name = fmt::format("{}_{:02}.sql", day, counter);
        if (!std::filesystem::exists(folder / name, code))
            return name;
    }
    return fmt::format("{}_99.sql", day);
}

std::optional<std::filesystem::path> WorldEditJournal::Export(std::filesystem::path const& folder, std::string& error) const
{
    std::vector<WorldEdit> const edits = Entries();
    if (edits.empty())
    {
        error = "nothing has been edited while this server has been running, so there is nothing to export";
        return std::nullopt;
    }

    std::error_code code;
    std::filesystem::create_directories(folder, code);
    if (code)
    {
        error = fmt::format("{} could not be made: {}", ConfigMgr::PathToUtf8(folder), code.message());
        return std::nullopt;
    }

    std::filesystem::path const file = folder / NextFileName(folder, edits.back().EpochMs);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = fmt::format("{} could not be opened to write", ConfigMgr::PathToUtf8(file));
        return std::nullopt;
    }
    std::string const text = Render(edits);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (!out)
    {
        error = fmt::format("{} was not written whole", ConfigMgr::PathToUtf8(file));
        return std::nullopt;
    }
    LOG_INFO("server.reload", "Exported {} world edit(s) to {}", edits.size(), ConfigMgr::PathToUtf8(file));
    return file;
}
