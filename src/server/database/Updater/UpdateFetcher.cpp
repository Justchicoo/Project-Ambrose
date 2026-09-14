/*
 * Project Ambrose by Imjustchico
 * Reads include paths with $ as the source folder, refuses missing released folders, badly named released files and duplicates, hashes files with CRLF normalized to LF, and applies pending updates released first, then by name.
 */

#include "UpdateFetcher.h"
#include "ConfigMgr.h"
#include "Hex.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "SHA256.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <set>

UpdateFetcher::UpdateFetcher(MySQLConnection& bookkeeping, std::filesystem::path sourceDirectory, ApplyFunction apply)
    : _bookkeeping(bookkeeping), _sourceDirectory(std::move(sourceDirectory)), _apply(std::move(apply))
{
}

std::string_view UpdateFetcher::ToString(UpdateState state) noexcept
{
    switch (state)
    {
        case UpdateState::Released: return "RELEASED";
        case UpdateState::Custom: return "CUSTOM";
        case UpdateState::Module: return "MODULE";
        case UpdateState::Archived: return "ARCHIVED";
        case UpdateState::Pending: return "PENDING";
    }
    return "RELEASED";
}

std::optional<UpdateState> UpdateFetcher::ParseState(std::string_view text) noexcept
{
    for (UpdateState const state : { UpdateState::Released, UpdateState::Custom, UpdateState::Module, UpdateState::Archived, UpdateState::Pending })
        if (ToString(state) == text)
            return state;
    return std::nullopt;
}

int UpdateFetcher::GetOrder(UpdateState state) noexcept
{
    switch (state)
    {
        case UpdateState::Archived:
        case UpdateState::Released: return 0;
        case UpdateState::Pending: return 1;
        case UpdateState::Module: return 2;
        case UpdateState::Custom: return 3;
    }
    return 3;
}

std::string UpdateFetcher::HashContents(std::string_view contents)
{
    SHA256 hash;
    std::size_t start = 0;
    while (start < contents.size())
    {
        std::size_t const carriage = contents.find('\r', start);
        if (carriage == std::string_view::npos)
        {
            hash.Update(contents.substr(start));
            break;
        }
        hash.Update(contents.substr(start, carriage - start));
        if (carriage + 1 >= contents.size() || contents[carriage + 1] != '\n')
            hash.Update(std::string_view("\r"));
        start = carriage + 1;
    }
    SHA256::Digest const digest = hash.Finalize();
    return Hex::Encode(digest);
}

bool UpdateFetcher::IsReleasedFileName(std::string_view fileName) noexcept
{
    if (fileName.size() != 17 || !Ambrose::EqualsIgnoreCase(fileName.substr(13), ".sql"))
        return false;
    for (std::size_t i = 0; i < 13; ++i)
    {
        bool const separator = i == 4 || i == 7 || i == 10;
        if (separator ? fileName[i] != '_' : (fileName[i] < '0' || fileName[i] > '9'))
            return false;
    }
    return true;
}

bool UpdateFetcher::HasSqlExtension(std::filesystem::path const& path)
{
    return Ambrose::EqualsIgnoreCase(ConfigMgr::PathToUtf8(path.extension()), ".sql");
}

bool UpdateFetcher::ReadFile(std::filesystem::path const& path, std::string& contents, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = fmt::format("cannot open {}", ConfigMgr::PathToUtf8(path));
        return false;
    }
    contents.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = fmt::format("cannot read {}", ConfigMgr::PathToUtf8(path));
        return false;
    }
    return true;
}

bool UpdateFetcher::ListSqlFiles(std::filesystem::path const& directory, std::vector<std::filesystem::path>& files, std::string& error)
{
    files.clear();
    std::error_code code;
    std::filesystem::directory_iterator iterator(directory, code);
    for (; !code && iterator != std::filesystem::directory_iterator(); iterator.increment(code))
    {
        bool const regular = iterator->is_regular_file(code);
        if (code)
            break;
        if (regular && HasSqlExtension(iterator->path()))
            files.push_back(iterator->path());
    }
    if (code)
    {
        error = fmt::format("cannot list {}: {}", ConfigMgr::PathToUtf8(directory), code.message());
        return false;
    }
    std::sort(files.begin(), files.end());
    return true;
}

bool UpdateFetcher::CollectFiles(std::vector<UpdateFile>& files, std::string& error) const
{
    files.clear();
    QueryResult const includes = _bookkeeping.Query("SELECT `path`, `state` FROM `updates_include`");
    if (_bookkeeping.GetLastErrorCode() != 0)
    {
        error = fmt::format("cannot read updates_include: [{}] {}", _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText());
        return false;
    }
    if (!includes)
        return true;

    std::set<std::string> names;
    do
    {
        std::string const text = (*includes)[0].Get<std::string>();
        std::string const stateText = (*includes)[1].Get<std::string>();
        std::optional<UpdateState> const state = ParseState(stateText);
        if (!state)
        {
            error = fmt::format("updates_include has an unknown state '{}' for {}", stateText, text);
            return false;
        }
        std::string_view relative = text;
        bool const fromSource = !relative.empty() && relative.front() == '$';
        if (fromSource)
        {
            relative.remove_prefix(1);
            while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\'))
                relative.remove_prefix(1);
        }
        std::filesystem::path const directory = fromSource ? _sourceDirectory / ConfigMgr::PathFromUtf8(relative) : ConfigMgr::PathFromUtf8(text);
        std::error_code directoryError;
        if (!std::filesystem::is_directory(directory, directoryError))
        {
            if (*state == UpdateState::Released || *state == UpdateState::Archived)
            {
                error = fmt::format("the {} update folder {} does not exist; set Updates.SourcePath to the Project Ambrose folder", ToString(*state), ConfigMgr::PathToUtf8(directory));
                return false;
            }
            LOG_DEBUG("sql.updates", "{} update folder {} does not exist and is skipped", ToString(*state), ConfigMgr::PathToUtf8(directory));
            continue;
        }
        std::vector<std::filesystem::path> paths;
        if (!ListSqlFiles(directory, paths, error))
            return false;
        for (std::filesystem::path const& path : paths)
        {
            std::string const name = ConfigMgr::PathToUtf8(path.filename());
            if ((*state == UpdateState::Released || *state == UpdateState::Archived) && !IsReleasedFileName(name))
            {
                error = fmt::format("{} in {} is not named YYYY_MM_DD_NN.sql", name, ConfigMgr::PathToUtf8(directory));
                return false;
            }
            if (name.size() > MaxNameLength)
            {
                error = fmt::format("{} is longer than {} characters", name, MaxNameLength);
                return false;
            }
            if (!names.insert(name).second)
            {
                error = fmt::format("{} exists in more than one update folder", name);
                return false;
            }
            files.push_back(UpdateFile{ path, name, *state });
        }
    }
    while (includes->NextRow());

    std::sort(files.begin(), files.end(), [](UpdateFile const& left, UpdateFile const& right)
    {
        int const leftOrder = GetOrder(left.State);
        int const rightOrder = GetOrder(right.State);
        return leftOrder != rightOrder ? leftOrder < rightOrder : left.Name < right.Name;
    });
    return true;
}

UpdateSummary UpdateFetcher::Update(std::string_view databaseLabel)
{
    UpdateSummary summary;
    std::vector<UpdateFile> files;
    std::string error;
    if (!CollectFiles(files, error))
    {
        LOG_ERROR("sql.updates", "Cannot update the {} database: {}", databaseLabel, error);
        summary.Succeeded = false;
        return summary;
    }

    std::map<std::string, std::string, std::less<>> applied;
    QueryResult const recorded = _bookkeeping.Query("SELECT `name`, `hash` FROM `updates`");
    if (_bookkeeping.GetLastErrorCode() != 0)
    {
        LOG_ERROR("sql.updates", "Cannot read the updates table of the {} database: [{}] {}", databaseLabel, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText());
        summary.Succeeded = false;
        return summary;
    }
    if (recorded)
    {
        do
            applied.emplace((*recorded)[0].Get<std::string>(), (*recorded)[1].Get<std::string>());
        while (recorded->NextRow());
    }

    std::set<std::string, std::less<>> present;
    for (UpdateFile const& file : files)
        present.insert(file.Name);
    std::map<std::string, std::string, std::less<>> vanishedByHash;
    for (auto const& [name, recordedHash] : applied)
        if (!present.contains(name) && !recordedHash.empty())
            vanishedByHash.emplace(recordedHash, name);

    for (UpdateFile const& file : files)
    {
        std::string contents;
        if (!ReadFile(file.Path, contents, error))
        {
            LOG_ERROR("sql.updates", "Cannot update the {} database: {}", databaseLabel, error);
            summary.Succeeded = false;
            return summary;
        }
        std::string const hash = HashContents(contents);
        if (auto const found = applied.find(file.Name); found != applied.end())
        {
            ++summary.AlreadyApplied;
            if (!found->second.empty() && found->second != hash)
            {
                ++summary.Changed;
                LOG_WARN("sql.updates", "{} changed after it was applied to the {} database; the recorded hash is {}, the file's is {}", file.Name, databaseLabel, found->second, hash);
            }
            continue;
        }
        if (auto const renamed = vanishedByHash.find(hash); renamed != vanishedByHash.end())
        {
            std::string const rename = fmt::format("UPDATE `updates` SET `name` = '{}', `state` = '{}' WHERE `name` = '{}'",
                _bookkeeping.Escape(file.Name), ToString(file.State), _bookkeeping.Escape(renamed->second));
            if (!_bookkeeping.Execute(rename))
            {
                LOG_ERROR("sql.updates", "Cannot record that {} was renamed to {} in the {} database: [{}] {}", renamed->second, file.Name, databaseLabel, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText());
                summary.Succeeded = false;
                return summary;
            }
            LOG_INFO("sql.updates", "{} was already applied to the {} database as {}; recorded the new name", file.Name, databaseLabel, renamed->second);
            applied.emplace(file.Name, hash);
            vanishedByHash.erase(renamed);
            ++summary.AlreadyApplied;
            continue;
        }

        auto const start = std::chrono::steady_clock::now();
        if (!_apply(file, contents))
        {
            summary.Succeeded = false;
            return summary;
        }
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::string const record = fmt::format("INSERT INTO `updates` (`name`, `hash`, `state`, `speed`) VALUES ('{}', '{}', '{}', {})",
            _bookkeeping.Escape(file.Name), hash, ToString(file.State), elapsed);
        if (!_bookkeeping.Execute(record))
        {
            LOG_ERROR("sql.updates", "{} was applied to the {} database but could not be recorded: [{}] {}", file.Name, databaseLabel, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText());
            summary.Succeeded = false;
            return summary;
        }
        applied.emplace(file.Name, hash);
        ++summary.Applied;
        LOG_INFO("sql.updates", "Applied {} {} to the {} database in {} ms", ToString(file.State), file.Name, databaseLabel, elapsed);
    }

    if (summary.Applied == 0)
        LOG_INFO("sql.updates", "The {} database is up to date", databaseLabel);
    else
        LOG_INFO("sql.updates", "Applied {} update(s) to the {} database", summary.Applied, databaseLabel);
    return summary;
}
