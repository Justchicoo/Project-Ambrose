/*
 * Project Ambrose by Imjustchico
 * Reads include paths with $ as the source folder, refuses missing released folders, badly named released files and duplicates, hashes files with CRLF normalized to LF, and applies pending updates released first, then by name, passing again after any pass that applied something, so the files of a folder an update adds are applied in the same run; a pass that meets a file the caller does not admit stops there, so nothing after it runs ahead of it, and the listing reads the same files and records the same way without changing either.
 */

#include "UpdateFetcher.h"
#include "ConfigMgr.h"
#include "Hex.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "SHA256.h"
#include "SqlScript.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

UpdateFetcher::UpdateFetcher(MySQLConnection& bookkeeping, UpdaterSettings settings, ApplyFunction apply)
    : _bookkeeping(bookkeeping), _settings(std::move(settings)), _apply(std::move(apply))
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

bool UpdateFetcher::IsPendingFileName(std::string_view fileName) noexcept
{
    if (fileName.size() < 10 || fileName.substr(0, 4) != "rev_" || !Ambrose::EqualsIgnoreCase(fileName.substr(fileName.size() - 4), ".sql"))
        return false;
    std::size_t const separator = fileName.find('_', 4);
    if (separator == std::string_view::npos || separator == 4 || separator + 1 >= fileName.size() - 4)
        return false;
    for (std::size_t index = 4; index < separator; ++index)
        if (fileName[index] < '0' || fileName[index] > '9')
            return false;
    for (std::size_t index = separator + 1; index < fileName.size() - 4; ++index)
        if (!((fileName[index] >= 'a' && fileName[index] <= 'z') || (fileName[index] >= 'A' && fileName[index] <= 'Z') || (fileName[index] >= '0' && fileName[index] <= '9') || fileName[index] == '-' || fileName[index] == '_'))
            return false;
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

UpdateClassification UpdateFetcher::Classify(std::string_view contents)
{
    UpdateClassification result;
    std::vector<SqlScript::Statement> statements;
    std::string error;
    if (!SqlScript::Split(contents, statements, error))
    {
        result.Kind = UpdateKind::Schema;
        result.Transactional = false;
        result.Problem = std::move(error);
        return result;
    }
    for (SqlScript::Statement const& statement : statements)
    {
        if (SqlScript::IsDataOnly(statement.Text))
        {
            std::string const keyword = SqlScript::LeadingKeyword(statement.Text);
            if (!keyword.empty() && keyword != "INSERT" && keyword != "REPLACE" && keyword != "UPDATE" && keyword != "DELETE" && keyword != "SELECT" && keyword != "WITH")
                result.Transactional = false;
            continue;
        }
        result.Kind = UpdateKind::Schema;
        result.Transactional = false;
        result.Line = statement.Line;
        result.Statement = SqlScript::Excerpt(statement.Text, ExcerptLength);
        return result;
    }
    return result;
}

bool UpdateFetcher::IsDataOnly(std::string_view contents)
{
    return Classify(contents).Kind == UpdateKind::Data;
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
        std::filesystem::path const directory = fromSource ? _settings.SourceDirectory / ConfigMgr::PathFromUtf8(relative) : ConfigMgr::PathFromUtf8(text);
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
            if (*state == UpdateState::Pending)
            {
                if (!_settings.AllowPending)
                    continue;
                if (!IsPendingFileName(name))
                {
                    error = fmt::format("{} in {} is not named rev_<unix-timestamp>_<slug>.sql", name, ConfigMgr::PathToUtf8(directory));
                    return false;
                }
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

bool UpdateFetcher::ReadRecorded(std::vector<AppliedUpdate>& recorded, std::string& error) const
{
    recorded.clear();
    QueryResult const rows = _bookkeeping.Query("SELECT `name`, `hash`, `state`, UNIX_TIMESTAMP(`timestamp`), `speed` FROM `updates` ORDER BY `name`");
    if (_bookkeeping.GetLastErrorCode() != 0)
    {
        error = fmt::format("cannot read the updates table: [{}] {}", _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText());
        return false;
    }
    if (!rows)
        return true;
    do
    {
        AppliedUpdate row;
        row.Name = (*rows)[0].Get<std::string>();
        row.Hash = (*rows)[1].Get<std::string>();
        std::string const stateText = (*rows)[2].Get<std::string>();
        std::optional<UpdateState> const state = ParseState(stateText);
        if (!state)
        {
            error = fmt::format("the updates table has an unknown state '{}' for {}", stateText, row.Name);
            return false;
        }
        row.State = *state;
        row.AppliedAt = (*rows)[3].Get<int64>();
        row.Milliseconds = (*rows)[4].Get<uint64>();
        recorded.push_back(std::move(row));
    }
    while (rows->NextRow());
    return true;
}

bool UpdateFetcher::Inspect(UpdateReport& report, std::string& error) const
{
    report = UpdateReport{};
    std::vector<UpdateFile> files;
    if (!CollectFiles(files, error))
        return false;
    std::vector<AppliedUpdate> recorded;
    if (!ReadRecorded(recorded, error))
        return false;

    std::map<std::string, std::size_t, std::less<>> byName;
    for (std::size_t index = 0; index < recorded.size(); ++index)
        byName.emplace(recorded[index].Name, index);
    std::set<std::string, std::less<>> present;
    for (UpdateFile const& file : files)
        present.insert(file.Name);
    std::map<std::string, std::string, std::less<>> vanishedByHash;
    for (AppliedUpdate const& row : recorded)
        if (!present.contains(row.Name) && !row.Hash.empty())
            vanishedByHash.emplace(row.Hash, row.Name);

    for (UpdateFile const& file : files)
    {
        std::string contents;
        if (!ReadFile(file.Path, contents, error))
            return false;
        std::string const hash = HashContents(contents);
        if (auto const found = byName.find(file.Name); found != byName.end())
        {
            AppliedUpdate& row = recorded[found->second];
            row.Present = true;
            row.Changed = !row.Hash.empty() && row.Hash != hash;
            continue;
        }
        PendingUpdate pending;
        pending.File = file;
        pending.Hash = hash;
        if (auto const renamed = vanishedByHash.find(hash); renamed != vanishedByHash.end())
        {
            pending.RenamedFrom = renamed->second;
            vanishedByHash.erase(renamed);
        }
        else
            pending.Classification = Classify(contents);
        report.Pending.push_back(std::move(pending));
    }
    report.Applied = std::move(recorded);
    return true;
}

UpdateSummary UpdateFetcher::Update(std::string_view databaseLabel, AdmitFunction const& admit)
{
    UpdateSummary summary;
    std::set<std::string, std::less<>> warned;
    for (std::size_t pass = 0; pass < MaxPasses; ++pass)
    {
        UpdateSummary ran = Pass(databaseLabel, warned, admit);
        summary.Applied += ran.Applied;
        std::move(ran.AppliedNames.begin(), ran.AppliedNames.end(), std::back_inserter(summary.AppliedNames));
        if (!ran.Succeeded)
        {
            summary.Succeeded = false;
            summary.FailedAt = std::move(ran.FailedAt);
            summary.Failure = std::move(ran.Failure);
            return summary;
        }
        if (ran.Applied != 0)
            continue;
        summary.AlreadyApplied = ran.AlreadyApplied;
        summary.Changed = ran.Changed;
        summary.StoppedAt = std::move(ran.StoppedAt);
        if (!summary.StoppedAt.empty())
            LOG_INFO("sql.updates", "Applied {} data-only update(s) to the {} database; {} can change the schema, so it and every update after it wait for the next start", summary.Applied, databaseLabel, summary.StoppedAt);
        else if (summary.Applied == 0)
            LOG_INFO("sql.updates", "The {} database is up to date", databaseLabel);
        else
            LOG_INFO("sql.updates", "Applied {} update(s) to the {} database", summary.Applied, databaseLabel);
        return summary;
    }
    LOG_ERROR("sql.updates", "The {} database still had updates to apply after {} passes, each finding files the last one did not", databaseLabel, MaxPasses);
    summary.Succeeded = false;
    summary.Failure = fmt::format("updates were still being found after {} passes", MaxPasses);
    return summary;
}

UpdateSummary UpdateFetcher::Pass(std::string_view databaseLabel, std::set<std::string, std::less<>>& warned, AdmitFunction const& admit)
{
    UpdateSummary summary;
    auto const fail = [&summary, databaseLabel](std::string failure, std::string file = {})
    {
        LOG_ERROR("sql.updates", "Cannot update the {} database: {}", databaseLabel, failure);
        summary.Succeeded = false;
        summary.FailedAt = std::move(file);
        summary.Failure = std::move(failure);
        return summary;
    };
    std::vector<UpdateFile> files;
    std::string error;
    if (!CollectFiles(files, error))
        return fail(std::move(error));

    std::vector<AppliedUpdate> recorded;
    if (!ReadRecorded(recorded, error))
        return fail(std::move(error));
    std::map<std::string, std::string, std::less<>> applied;
    for (AppliedUpdate& row : recorded)
        applied.emplace(std::move(row.Name), std::move(row.Hash));

    std::set<std::string, std::less<>> present;
    for (UpdateFile const& file : files)
        present.insert(file.Name);
    std::map<std::string, std::string, std::less<>> vanishedByHash;
    for (auto const& [name, recordedHash] : applied)
        if (!present.contains(name) && !recordedHash.empty())
            vanishedByHash.emplace(recordedHash, name);
    std::set<std::string, std::less<>> deadNames;
    for (auto const& [name, recordedHash] : applied)
        if (!present.contains(name))
            deadNames.insert(name);
    std::map<std::string, std::string, std::less<>> fileHashes;
    for (UpdateFile const& file : files)
    {
        std::string contents;
        if (!ReadFile(file.Path, contents, error))
            return fail(std::move(error), file.Name);
        fileHashes.emplace(file.Name, HashContents(contents));
    }
    for (auto const& [name, hash] : fileHashes)
        if (auto const renamed = vanishedByHash.find(hash); renamed != vanishedByHash.end())
            deadNames.erase(renamed->second);
    for (std::string const& name : deadNames)
        if (warned.insert(name).second)
            LOG_WARN("sql.updates", "{} was applied to the {} database but is no longer present on disk", name, databaseLabel);
    if (_settings.CleanDeadRefMaxCount > 0 && deadNames.size() > static_cast<std::size_t>(_settings.CleanDeadRefMaxCount))
        return fail(fmt::format("{} applied update references are missing from disk (limit {})", deadNames.size(), _settings.CleanDeadRefMaxCount));
    if (_settings.CleanDeadRefMaxCount != 0)
        for (std::string const& name : deadNames)
            if (!_bookkeeping.Execute(fmt::format("DELETE FROM `updates` WHERE `name` = '{}'", _bookkeeping.Escape(name))))
                return fail(fmt::format("cannot remove missing update {}: [{}] {}", name, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText()));

    for (UpdateFile const& file : files)
    {
        std::string const hash = fileHashes.at(file.Name);
        bool reapply = false;
        if (auto const found = applied.find(file.Name); found != applied.end())
        {
            ++summary.AlreadyApplied;
            if (!found->second.empty() && found->second != hash)
            {
                ++summary.Changed;
                if (!_settings.Redundancy)
                    return fail(fmt::format("{} changed after it was applied to the {} database; Updates.Redundancy is disabled", file.Name, databaseLabel), file.Name);
                reapply = true;
            }
            else if (found->second.empty())
            {
                if (!_settings.AllowRehash)
                    return fail(fmt::format("{} has no recorded hash; enable Updates.AllowRehash to fill it", file.Name), file.Name);
                if (!_bookkeeping.Execute(fmt::format("UPDATE `updates` SET `hash` = '{}' WHERE `name` = '{}'", hash, _bookkeeping.Escape(file.Name))))
                    return fail(fmt::format("cannot fill the hash for {}: [{}] {}", file.Name, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText()), file.Name);
                LOG_INFO("sql.updates", "Filled the missing hash for {} in the {} database", file.Name, databaseLabel);
                continue;
            }
            if (!reapply)
                continue;
        }
        if (auto const renamed = vanishedByHash.find(hash); renamed != vanishedByHash.end())
        {
            std::string const rename = fmt::format("UPDATE `updates` SET `name` = '{}', `state` = '{}' WHERE `name` = '{}'",
                _bookkeeping.Escape(file.Name), ToString(file.State), _bookkeeping.Escape(renamed->second));
            if (!_bookkeeping.Execute(rename))
                return fail(fmt::format("cannot record that {} was renamed to {}: [{}] {}", renamed->second, file.Name, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText()), file.Name);
            LOG_INFO("sql.updates", "{} was already applied to the {} database as {}; recorded the new name", file.Name, databaseLabel, renamed->second);
            applied.emplace(file.Name, hash);
            vanishedByHash.erase(renamed);
            ++summary.AlreadyApplied;
            continue;
        }

        std::string contents;
        if (!ReadFile(file.Path, contents, error))
            return fail(std::move(error), file.Name);
        if (admit && !admit(file, contents))
        {
            summary.StoppedAt = file.Name;
            return summary;
        }
        auto const start = std::chrono::steady_clock::now();
        std::string failure;
        if (!_apply(file, contents, failure))
        {
            summary.Succeeded = false;
            summary.FailedAt = file.Name;
            summary.Failure = failure.empty() ? fmt::format("{} could not be applied", file.Name) : std::move(failure);
            return summary;
        }
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::string const record = reapply
            ? fmt::format("UPDATE `updates` SET `hash` = '{}', `state` = '{}', `timestamp` = CURRENT_TIMESTAMP, `speed` = {} WHERE `name` = '{}'",
                hash, ToString(file.State), elapsed, _bookkeeping.Escape(file.Name))
            : fmt::format("INSERT INTO `updates` (`name`, `hash`, `state`, `speed`) VALUES ('{}', '{}', '{}', {})",
                _bookkeeping.Escape(file.Name), hash, ToString(file.State), elapsed);
        if (!_bookkeeping.Execute(record))
            return fail(fmt::format("{} was applied but could not be recorded: [{}] {}", file.Name, _bookkeeping.GetLastErrorCode(), _bookkeeping.GetLastErrorText()), file.Name);
        applied.emplace(file.Name, hash);
        ++summary.Applied;
        summary.AppliedNames.push_back(file.Name);
        LOG_INFO("sql.updates", "Applied {} {} to the {} database in {} ms", ToString(file.State), file.Name, databaseLabel, elapsed);
    }
    return summary;
}
